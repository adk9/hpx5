#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>

#include "hpx/hpx.h"
#include "libsync/queues.h"
#include "pxgl/adjacency_list.h"
#include "pxgl/bfs.h"
#include "libpxgl/termination.h"

adj_list_t bfs_graph = HPX_NULL;

two_lock_queue_t* current_set_q;
two_lock_queue_t* next_set_q;

distance_t local_level = 0;

hpx_action_t increment_level = HPX_ACTION_INVALID;
static int _increment_level_action() {
  ++local_level;
  return HPX_SUCCESS;
}

hpx_action_t bfs_init = HPX_ACTION_INVALID;
static int bfs_init_action() {
  
  current_set_q = sync_two_lock_queue_new();
  next_set_q = sync_two_lock_queue_new();

  sync_two_lock_queue_init(current_set_q, NULL);
  sync_two_lock_queue_init(next_set_q, NULL);
  
  // currently nothing
  return HPX_SUCCESS;
}

bool _try_mark_visited(adj_list_vertex_t *const vertex) {
  // printf("try update, vertex: %zu, distance: %zu\n", vertex, distance);
  color_t prev_color = sync_load(&vertex->color, SYNC_RELAXED);
  color_t old_color = prev_color;
  while (prev_color == COLOR_GRAY) {
    old_color = prev_color;
    prev_color = sync_cas_val(&vertex->color, prev_color, COLOR_WHITE, SYNC_RELAXED, SYNC_RELAXED);
    if(prev_color == old_color) return true;
    // sync_pause;
  }
  return false;
}

hpx_action_t bfs_initialize_graph = HPX_ACTION_INVALID;
static int _bfs_initialize_graph_action(const adj_list_t *g) {
  bfs_graph = *g;
  return HPX_SUCCESS;
}


hpx_action_t bfs_process_source_vertex = HPX_ACTION_INVALID;
static int _bfs_process_source_vertex_action(const hpx_addr_t *const source_address) {
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  // level is maintained in the distance field
  // we dont need locks here; source is updated synchronously
  vertex->distance = 0;
  vertex->color = COLOR_WHITE;

  // put source vertex to next set queue
  hpx_addr_t* enqtarget = (hpx_addr_t *)malloc(sizeof(hpx_addr_t));
  *enqtarget = *source_address;
  sync_two_lock_queue_enqueue(next_set_q, enqtarget); // put only the vertex_t

  hpx_gas_unpin(target);

  return HPX_SUCCESS;
}

hpx_action_t bfs_visit_source = HPX_ACTION_INVALID;
static int _bfs_visit_source_action() {
  // target is the global address in the vertex array
  // we fist need to get the local address pointed by target
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t vertex;
  hpx_addr_t *v;

  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  // now calling action with local memory 
  // TODO pass target as a parameter
  return hpx_call_sync(vertex, bfs_process_source_vertex, NULL, 0, &target, sizeof(hpx_addr_t *));
}

static int _copy_next_to_current() {
  size_t count = 0;

  assert(sync_two_lock_queue_dequeue(current_set_q) == NULL);

  hpx_addr_t* v = NULL;
  // Copy the next queue to current queue
  // pop from next queue and push it to current queue
  while((v = sync_two_lock_queue_dequeue(next_set_q))) {
    sync_two_lock_queue_enqueue(current_set_q, v); 
    ++count;
  }

  assert(sync_two_lock_queue_dequeue(next_set_q) == NULL);

  return count;
}

hpx_action_t process_neighbor_data = HPX_ACTION_INVALID;
static int _process_neighbor_data_action(const hpx_addr_t *const v) {
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  printf("Inside _process_neighbor_data_action %" PRIu64 "\n", *v);
  // we dont need locks here; source is updated synchronously
  // TODO this needs to be atomic
  if (_try_mark_visited(vertex)) {
    // we successfully marked the vertex as visited
    assert(vertex->color == COLOR_WHITE);

    // we do not need atomics update distance
    vertex->distance = local_level;
    printf("Inside _process_neighbor_data_action - level %" PRIu64 "\n", local_level);

    // put source vertex to next set queue
    hpx_addr_t* enqtarget = (hpx_addr_t *)malloc(sizeof(hpx_addr_t));
    *enqtarget = *v;
    sync_two_lock_queue_enqueue(next_set_q, enqtarget); // put only the vertex_t
  }

  hpx_gas_unpin(target);

  printf("Decrementing finish count by 1 \n");
  _increment_finished_count();
  return HPX_SUCCESS;
}

hpx_action_t process_neighbor = HPX_ACTION_INVALID;
static int _process_neighbor_action() {
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t vertex;
  hpx_addr_t *v;

  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  printf("Inside _process_neighbor_action - target %" PRIu64 "\n", target);

  return hpx_call_sync(vertex, process_neighbor_data, NULL, 0, &target, sizeof(hpx_addr_t *));
}

hpx_action_t process_vertex_data = HPX_ACTION_INVALID;
static int _process_vertex_data_action() {
  const hpx_addr_t target = hpx_thread_current_target();
  printf("Action invoked on address %" PRIu64 "\n", target);
  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  printf("Increasing termination counts by %zu.\n", vertex->num_edges);
  // increment active coutn per each adjacency
  _increment_active_count(vertex->num_edges);

  printf("Sending update to neighbors of vertex %zu\n", vertex);
  printf("Number of edges %d\n", vertex->num_edges);

  int i;
  for(i=0; i < vertex->num_edges; ++i) {
    adj_list_edge_t* e = &vertex->edge_list[i];

    // process destination e->dest;
    const hpx_addr_t index
      = hpx_addr_add(bfs_graph, e->dest * sizeof(hpx_addr_t), _index_array_block_size);

    hpx_call(index, process_neighbor, HPX_NULL, NULL, 0);
  }

  hpx_gas_unpin(target); // TODO find correct signature

  // we finished processing a vertex in the current queue; increase the finish count
  printf("Decrementing termination count by 1 \n");
  _increment_finished_count();
  
  return HPX_SUCCESS;
}

hpx_action_t process_vertex = HPX_ACTION_INVALID;
static int _process_vertex_action() {
  const hpx_addr_t target = hpx_thread_current_target();
  //  printf("Action invoked on address %" PRIu64 "\n", target);
  
  hpx_addr_t vertex;
  hpx_addr_t *v;

  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  return hpx_call_sync(vertex, process_vertex_data, NULL, 0, NULL, 0);
}

hpx_action_t process_current_q = HPX_ACTION_INVALID;
static int _process_current_q_action() {

  hpx_addr_t* v = NULL;
  while((v = sync_two_lock_queue_dequeue(current_set_q))) {
    // which is better ? HPX_HERE or like this ?
    printf("Invoking action on address %" PRIu64 "\n", v);
    hpx_call(*v, process_vertex, HPX_NULL, NULL, 0);
    free(v);
  }
  
  return HPX_SUCCESS;
}

// When calling this action we also copy next queue to 
// current queue.
hpx_action_t bfs_increase_active_counts = HPX_ACTION_INVALID;
static int _bfs_increase_active_counts_action(const void * args) {
  size_t no_tasks = _copy_next_to_current();
  printf("Increasing termination counts by %zu.\n", no_tasks);
  _increment_active_count(no_tasks);
  // printf("Active counts increased.\n");
  return HPX_SUCCESS;
}

hpx_action_t check_next_q = 0;
static int _check_next_q_action(const hpx_addr_t * const reduce_lco) {
  //TODO Ugly way to check whether empty. Need to change later
  int is_empty = 0;
  void* v = sync_two_lock_queue_dequeue(current_set_q);
  if (v == NULL) {
    is_empty = 1;
  } else {
    sync_two_lock_queue_enqueue(current_set_q, v);
  }

  printf("############### _check_next_q_action : q empty ? %d \n", is_empty);
  
  hpx_lco_set(*reduce_lco, sizeof(int), &is_empty, HPX_NULL, HPX_NULL);

  return HPX_SUCCESS;
} 

void op_empty_reduction(int* const output, const int* const input, const size_t bytes) {
  *output = *output & *input;
}

void init_empty_reduction(int* const output, const size_t bytes) {
  *output = 1;
}

int _are_all_next_qs_empty() {
  const hpx_addr_t reduce_lco = 
    hpx_lco_reduce_new(
			  HPX_LOCALITIES, 
			  sizeof(int), 
			  (hpx_monoid_id_t)init_empty_reduction,
			  (hpx_monoid_op_t)op_empty_reduction);
  const hpx_addr_t bcast_lco = hpx_lco_future_new(0);
  hpx_bcast(check_next_q, bcast_lco, &reduce_lco,
            sizeof(reduce_lco));
  //printf("Waiting for the find_next_level lco to be finished\n");
  hpx_lco_wait(bcast_lco);

  int are_all_qs_empty = 0;
  hpx_lco_get(reduce_lco, sizeof(are_all_qs_empty), &are_all_qs_empty);

  //printf("The lco for find_next_level has been set\n");
  hpx_lco_delete(bcast_lco, HPX_NULL);
  hpx_lco_delete(reduce_lco, HPX_NULL);

  return are_all_qs_empty;
}

hpx_action_t call_bfs = HPX_ACTION_INVALID;
int call_bfs_action(const call_bfs_args_t *const args) {

  const hpx_addr_t index
    = hpx_addr_add(args->graph, args->source * sizeof(hpx_addr_t), _index_array_block_size);

  // send graph to everyone
  printf("Initialize graph ... \n");
  const hpx_addr_t initialize_graph_lco = hpx_lco_future_new(0);
  hpx_bcast(bfs_initialize_graph, initialize_graph_lco, &args->graph,
            sizeof(args->graph));
  hpx_lco_wait(initialize_graph_lco);
  hpx_lco_delete(initialize_graph_lco, HPX_NULL);
  printf("Initialize graph-Done ... \n");
  // use distance to record the level
  //distance_t level = 0;

  printf("Visiting source node ... \n");
  // set source color and level
  // put source to next queue
  hpx_call_sync(index, bfs_visit_source, NULL, 0, NULL, 0);
  printf("Visiting source node-Done ... \n");

  while(true) {

    printf("Inside while loop \n");
    const hpx_addr_t termination_lco = hpx_lco_future_new(0);

    if (_get_termination() == COUNT_TERMINATION) {
      // increase the level
      //++level;
      hpx_bcast_sync(increment_level, NULL, 0);

      printf("Processing level %u", local_level);

      hpx_addr_t internal_termination_lco = hpx_lco_future_new(0);

      printf("Initialize termination for processing level %u\n", local_level);
      // initialize termination per each level
      hpx_bcast_sync(_initialize_termination_detection, NULL, 0);

      printf("Increasing active count for processing level %u\n", local_level);
      // copy next queue to current queue and increase the active count by the number of
      // elements in the current queue
      hpx_bcast_sync(bfs_increase_active_counts, NULL, 0);

      printf("Processing current queue for processing level %u\n", local_level);
      // process current queue in every locality
      hpx_bcast(process_current_q, HPX_NULL, NULL, 0);
    
      printf("Invoking termination for processing level %u\n", local_level);
      _detect_termination(termination_lco, internal_termination_lco);

      printf("Wait till terminations finishes for processing level %u\n", local_level);
      // wait till current level finishes
      hpx_lco_wait(termination_lco);
      hpx_lco_delete(termination_lco, HPX_NULL);

      printf("Checking whether for processing level %u\n", local_level);
      // find whether all next queues are empty
      // if all next queues are empty then terminate the algorithm
      if (_are_all_next_qs_empty()) {
	break;
      }
    } else {
      // process termination should be easy to add
      fprintf(stderr, "bfs: invalid termination mode. BFS only supports count termination.\n");
      hpx_abort();
    }
  }

  hpx_lco_set(args->termination_lco, 0, NULL, HPX_NULL, HPX_NULL);
  printf("BFS executed %zu levels.", local_level);
  return HPX_SUCCESS;
}


static HPX_CONSTRUCTOR void _bfs_register_actions() {
  HPX_REGISTER_ACTION(bfs_init_action, &bfs_init);
  HPX_REGISTER_ACTION(_increment_level_action, &increment_level);
  HPX_REGISTER_ACTION(_bfs_initialize_graph_action, &bfs_initialize_graph);
  HPX_REGISTER_ACTION(_bfs_visit_source_action, &bfs_visit_source);
  HPX_REGISTER_ACTION(_process_neighbor_action, &process_neighbor);
  HPX_REGISTER_ACTION(_process_neighbor_data_action, &process_neighbor_data);
  HPX_REGISTER_ACTION(_bfs_process_source_vertex_action, &bfs_process_source_vertex);
  HPX_REGISTER_ACTION(_process_vertex_action, &process_vertex);
  HPX_REGISTER_ACTION(_process_vertex_data_action, &process_vertex_data);
  HPX_REGISTER_ACTION(_process_current_q_action, &process_current_q);
  HPX_REGISTER_ACTION(_bfs_increase_active_counts_action, &bfs_increase_active_counts);
  HPX_REGISTER_ACTION(_check_next_q_action, &check_next_q);
  HPX_REGISTER_ACTION(call_bfs_action, &call_bfs);
}
