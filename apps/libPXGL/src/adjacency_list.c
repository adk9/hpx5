
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "libpxgl/termination.h"
#include "libpxgl/adjacency_list.h"

#define PXGL_ADJ_SYNC_ARG(orig, lco_name, arg_name) \
typedef struct { \
  orig; \
  hpx_addr_t lco_name; \
} arg_name##_args_t;

typedef uint32_t count_t;

hpx_addr_t count_array;
hpx_addr_t index_array;

sssp_uint_t _count_array_block_size = 0;
sssp_uint_t _index_array_block_size = 0;

static hpx_action_t _set_count_array_bsize;
static int _set_count_array_bsize_action(const uint32_t *arg) {
  _count_array_block_size = *arg;
  return HPX_SUCCESS;
}

static hpx_action_t _set_index_array_bsize;
static int _set_index_array_bsize_action(const uint32_t *arg) {
  _index_array_block_size = *arg;
  return HPX_SUCCESS;
}

// Action to increment count in the count array
static hpx_action_t _increment_count;
static int _increment_count_action(const void *const args) {
  const hpx_addr_t target = hpx_thread_current_target();

  //printf("_increment_count with %zu.\n", target);

  volatile count_t * const count;
  if (!hpx_gas_try_pin(target, (void**)&count))
    return HPX_RESEND;

  sync_fadd(count, 1, SYNC_RELAXED);

  _increment_finished_count();

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


// Action to count an edge
static hpx_action_t _count_edge;
static int _count_edge_action(const hpx_addr_t * const count_array) {
  const hpx_addr_t target = hpx_thread_current_target();

  // printf("_count_edge with %zu.\n", target);

  const edge_list_edge_t * const edge;
  if (!hpx_gas_try_pin(target, (void**)&edge))
    return HPX_RESEND;

  // if(*count_array != 272629760) printf("Huston, we have a problem: %zu.\n", *count_array);

  const hpx_addr_t count = hpx_addr_add(*count_array, edge->source * sizeof(count_t),
                                        _count_array_block_size);
  hpx_gas_unpin(target);

  return hpx_call(count, _increment_count, NULL, 0, HPX_NULL);
}


// Initialize vertex adjacency list size and distance = inf
static hpx_action_t _init_vertex;
static int _init_vertex_action(const hpx_addr_t * const vertices_sync) {
   const hpx_addr_t target = hpx_thread_current_target();

   adj_list_vertex_t *vertex;
   if (!hpx_gas_try_pin(target, (void**)&vertex))
     return HPX_RESEND;

   vertex->num_edges = 0;
   vertex->distance = SSSP_UINT_MAX;

   hpx_lco_set(*vertices_sync, 0, NULL, HPX_NULL, HPX_NULL);

   hpx_gas_unpin(target);
   return HPX_SUCCESS;
}

PXGL_ADJ_SYNC_ARG(count_t count, vertices_sync, _alloc_vertex)
static hpx_action_t _alloc_vertex;
static int _alloc_vertex_action(const _alloc_vertex_args_t * const args) {
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t *index;
  if (!hpx_gas_try_pin(target, (void**)&index))
    return HPX_RESEND;

  // We first pin the index and then allocate to get the blocks on the
  // same locality (at least initially)
  hpx_addr_t vertex = hpx_gas_alloc(sizeof(adj_list_vertex_t) + (args->count * sizeof(adj_list_edge_t)));

  *index = vertex;
  hpx_gas_unpin(target);

  return hpx_call(vertex, _init_vertex, &args->vertices_sync, sizeof(args->vertices_sync), HPX_NULL);
}


// Action to allocate an adjacency list entry: a vertex structure that
// holds the list of edges.
PXGL_ADJ_SYNC_ARG(hpx_addr_t index, vertices_sync, _alloc_adj_list_entry)
static hpx_action_t _alloc_adj_list_entry;
static int _alloc_adj_list_entry_action(const _alloc_adj_list_entry_args_t * const args)
{
  const hpx_addr_t target = hpx_thread_current_target();
  
  volatile count_t * const local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  count_t count = sync_load(local, SYNC_ACQUIRE);
  hpx_gas_unpin(target);

  const _alloc_vertex_args_t _alloc_vertex_args = { .count = count, .vertices_sync = args->vertices_sync };
  return hpx_call(args->index, _alloc_vertex, &_alloc_vertex_args, sizeof(_alloc_vertex_args), HPX_NULL);
}

static hpx_action_t _put_edge;
static int _put_edge_action(const adj_list_edge_t *edge)
{
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  // Increment the size of the vertex
  sssp_uint_t num_edges = sync_fadd(&vertex->num_edges, 1, SYNC_RELAXED);

  vertex->edge_list[num_edges] = *edge;

  _increment_finished_count();

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


PXGL_ADJ_SYNC_ARG(adj_list_edge_t edge, edges_sync, _put_edge_index)
static hpx_action_t _put_edge_index;
static int _put_edge_index_action(const adj_list_edge_t *edge)
{
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t vertex;
  hpx_addr_t *v;
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  return hpx_call(vertex, _put_edge, edge, sizeof(*edge), HPX_NULL);
}

static hpx_action_t _insert_edge;
static int _insert_edge_action(const hpx_addr_t * const index_array)
{
  const hpx_addr_t target = hpx_thread_current_target();

  edge_list_edge_t *edge;
  if (!hpx_gas_try_pin(target, (void**)&edge))
    return HPX_RESEND;

  const sssp_uint_t source = edge->source;

  adj_list_edge_t e;
  e.dest = edge->dest;
  e.weight = edge->weight;

  hpx_gas_unpin(target);

 // Then get the appropriate index array position to retrieve the
 // address of the vertex in the adjacency list
  const hpx_addr_t index
    = hpx_addr_add(*index_array, source * sizeof(hpx_addr_t), _index_array_block_size);

  // Insert the edge into the index array at the right index. Since we
  // call this action synchronously, we can safely send the stack
  // pointer out.
  return hpx_call(index, _put_edge_index, &e, sizeof(e), HPX_NULL);
}


static hpx_action_t _print_edge;
static int _print_edge_action(int *i)
{
  const hpx_addr_t target = hpx_thread_current_target();

  edge_list_edge_t *edge;
  if (!hpx_gas_try_pin(target, (void**)&edge))
    return HPX_RESEND;

  hpx_gas_unpin(target);
  printf("%d %" SSSP_UINT_PRI " %" SSSP_UINT_PRI " %" SSSP_UINT_PRI ".\n", *i, edge->source, edge->dest, edge->weight);
  return HPX_SUCCESS;
}


hpx_action_t adj_list_from_edge_list = 0;
int adj_list_from_edge_list_action(const edge_list_t * const el) {

  // Start allocating the index array.
  _index_array_block_size = ((el->num_vertices + HPX_LOCALITIES - 1) / HPX_LOCALITIES) * sizeof(hpx_addr_t);
  // printf("Index array block size is %" SSSP_UINT_PRI ".\n", _index_array_block_size); 
  hpx_addr_t index_arr_sync = hpx_lco_future_new(0);
  hpx_bcast(_set_index_array_bsize, &_index_array_block_size, sizeof(_index_array_block_size), index_arr_sync);

  // Array is allocated while the broadst of the block size is performed
  index_array = hpx_gas_global_alloc(HPX_LOCALITIES, _index_array_block_size);

  // Start allocating the count array for creating an edge histogram
  _count_array_block_size = ((el->num_vertices + HPX_LOCALITIES - 1) / HPX_LOCALITIES) * sizeof(count_t);
  hpx_addr_t count_arr_sync = hpx_lco_future_new(0);
  hpx_bcast(_set_count_array_bsize, &_count_array_block_size, sizeof(_count_array_block_size), count_arr_sync);

  // Array is allocated while the broadcast of the block size is performed
  count_array = hpx_gas_global_calloc(HPX_LOCALITIES, _count_array_block_size);
  // printf("count_array hpx address is %zu.\n", count_array);

  // Finish allocating the count array
  hpx_lco_wait(count_arr_sync);
  hpx_lco_delete(count_arr_sync, HPX_NULL);

  printf("Count the number of edges per source vertex\n");
  hpx_time_t now = hpx_time_now();

  hpx_addr_t init_termination_count_lco = hpx_lco_future_new(0);
  // printf("Starting initialization bcast.\n");
  hpx_bcast(_initialize_termination_detection, NULL, 0, init_termination_count_lco);
  // printf("Waiting on bcast.\n");
  hpx_lco_wait(init_termination_count_lco);
  hpx_lco_delete(init_termination_count_lco, HPX_NULL);

  edge_list_edge_t *edge_list;
  hpx_gas_try_pin(el->edge_list, (void**)&edge_list);
  // printf("edge_list is %" PRIxPTR " with source %" SSSP_UINT_PRI ", target %" SSSP_UINT_PRI ", and weight %" SSSP_UINT_PRI ".\n", edge_list, edge_list->source, edge_list->dest, edge_list->weight);
  //#include <sys/mman.h>
  //mprotect(edge_list, el->edge_list_bsize * 24, PROT_READ);

  // Count the number of edges per source vertex
  //for (int i = 0; i < el->num_edges; ++i) {
    //hpx_addr_t edge = hpx_addr_add(el->edge_list, i * sizeof(edge_list_edge_t), el->edge_list_bsize);
   //hpx_call(edge, _count_edge, &count_array, sizeof(count_array), HPX_NULL);
   //}
  hpx_count_range_call(_count_edge, el->edge_list, el->num_edges, sizeof(edge_list_edge_t), el->edge_list_bsize, sizeof(count_array), &count_array);
  double elapsed = hpx_time_elapsed_ms(now)/1e3;
  printf("Time elapsed in the loop: %f\n", elapsed);
  now = hpx_time_now();
  hpx_addr_t edges_sync = hpx_lco_and_new(2);
  _increment_active_count(el->num_edges);
  _detect_termination(edges_sync, edges_sync);
  hpx_lco_wait(edges_sync);
  elapsed = hpx_time_elapsed_ms(now)/1e3;
  printf("Time elapsed waiting for completion: %f\n", elapsed);
  hpx_lco_delete(edges_sync, HPX_NULL);

  // Finish allocating the index array now
  hpx_lco_wait(index_arr_sync);
  hpx_lco_delete(index_arr_sync, HPX_NULL);

  printf("Allocate the adjacency list according to the count of edges per vertex\n");
  // Allocate the adjacency list according to the count of edges per vertex
  now = hpx_time_now();
  hpx_addr_t vertices_alloc_sync = hpx_lco_and_new(el->num_vertices);
  for (int i = 0; i < el->num_vertices; ++i) {
    hpx_addr_t count = hpx_addr_add(count_array, i * sizeof(count_t), _count_array_block_size);
    hpx_addr_t index = hpx_addr_add(index_array, i * sizeof(hpx_addr_t), _index_array_block_size);
    const _alloc_adj_list_entry_args_t _alloc_adj_list_entry_args = { 
      .index = index, 
      .vertices_sync = vertices_alloc_sync 
    };
    hpx_call(count, _alloc_adj_list_entry, &_alloc_adj_list_entry_args, 
	     sizeof(_alloc_adj_list_entry_args), HPX_NULL);
  }
  elapsed = hpx_time_elapsed_ms(now)/1e3;
  printf("Time elapsed in the loop: %f\n", elapsed);
  now = hpx_time_now();
  hpx_lco_wait(vertices_alloc_sync);
  elapsed = hpx_time_elapsed_ms(now)/1e3;
  printf("Time elapsed waiting for completion: %f\n", elapsed);
  hpx_lco_delete(vertices_alloc_sync, HPX_NULL);

  printf("Convert edges to adjacencies\n");
  // For each edge in the edge list, we add it as an adjacency to a
  // vertex
  now = hpx_time_now();

  init_termination_count_lco = hpx_lco_future_new(0);
  // printf("Starting initialization bcast.\n");
  hpx_bcast(_initialize_termination_detection, NULL, 0, init_termination_count_lco);
  // printf("Waiting on bcast.\n");
  hpx_lco_wait(init_termination_count_lco);
  hpx_lco_delete(init_termination_count_lco, HPX_NULL);

  /* for (int i = 0; i < el->num_edges; ++i) { */
  /*   hpx_addr_t edge = hpx_addr_add(el->edge_list, i * sizeof(edge_list_edge_t), el->edge_list_bsize); */
  /*   _insert_edge_args_t _insert_edge_args = { */
  /*     .index_array = index_array, */
  /*     .edges_sync = edges_sync */
  /*   }; */
  /*   hpx_call(edge, _insert_edge, &_insert_edge_args,  */
  /* 	     sizeof(_insert_edge_args), HPX_NULL); */
  /* } */
  hpx_count_range_call(_insert_edge, el->edge_list, el->num_edges, sizeof(edge_list_edge_t), el->edge_list_bsize, sizeof(index_array), &index_array);
  elapsed = hpx_time_elapsed_ms(now)/1e3;
  printf("Time elapsed in the loop: %f\n", elapsed);
  now = hpx_time_now();
  edges_sync = hpx_lco_and_new(2);
  _increment_active_count(el->num_edges);
  _detect_termination(edges_sync, edges_sync);
  hpx_lco_wait(edges_sync);
  elapsed = hpx_time_elapsed_ms(now)/1e3;
  printf("Time elapsed waiting for completion: %f\n", elapsed);
  hpx_lco_delete(edges_sync, HPX_NULL);

  HPX_THREAD_CONTINUE(index_array);
  return HPX_SUCCESS;
}


static hpx_action_t _init_vertex_distance;
static int _init_vertex_distance_action(void *arg) {
   const hpx_addr_t target = hpx_thread_current_target();

   adj_list_vertex_t *vertex;
   if (!hpx_gas_try_pin(target, (void**)&vertex))
     return HPX_RESEND;

   vertex->distance = SSSP_UINT_MAX;

   hpx_gas_unpin(target);
   return HPX_SUCCESS;
}


hpx_action_t _reset_vertex = 0;
static int _reset_vertex_action(void *args) {
  const hpx_addr_t target = hpx_thread_current_target();
  hpx_addr_t vertex;
  hpx_addr_t *v;
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);
  return hpx_call_sync(vertex, _init_vertex_distance, NULL, 0, NULL, 0);
}


int reset_adj_list(adj_list_t adj_list, edge_list_t *el) {
  hpx_addr_t vertices = hpx_lco_and_new(el->num_vertices);
  for (int i = 0; i < el->num_vertices; ++i) {
    hpx_addr_t index = hpx_addr_add(index_array, i * sizeof(hpx_addr_t), _index_array_block_size);
    hpx_call(index, _reset_vertex, NULL, 0, vertices);
  }
  hpx_lco_wait(vertices);
  hpx_lco_delete(vertices, HPX_NULL);
  return HPX_SUCCESS;
}


hpx_action_t free_adj_list = 0;
int free_adj_list_action(void *arg) {
  hpx_gas_free(count_array, HPX_NULL);
  hpx_gas_free(index_array, HPX_NULL);
  return HPX_SUCCESS;
}


static __attribute__((constructor)) void _adj_list_register_actions() {
  HPX_REGISTER_ACTION(adj_list_from_edge_list_action,
                      &adj_list_from_edge_list);
  HPX_REGISTER_ACTION(free_adj_list_action, &free_adj_list);
  HPX_REGISTER_ACTION(_increment_count_action, &_increment_count);
  HPX_REGISTER_ACTION(_set_count_array_bsize_action, &_set_count_array_bsize);
  HPX_REGISTER_ACTION(_set_index_array_bsize_action, &_set_index_array_bsize);
  HPX_REGISTER_ACTION(_count_edge_action, &_count_edge);
  HPX_REGISTER_ACTION(_init_vertex_action, &_init_vertex);
  HPX_REGISTER_ACTION(_init_vertex_distance_action, &_init_vertex_distance);
  HPX_REGISTER_ACTION(_reset_vertex_action, &_reset_vertex);
  HPX_REGISTER_ACTION(_alloc_vertex_action, &_alloc_vertex);
  HPX_REGISTER_ACTION(_alloc_adj_list_entry_action, &_alloc_adj_list_entry);
  HPX_REGISTER_ACTION(_print_edge_action, &_print_edge);
  HPX_REGISTER_ACTION(_insert_edge_action, &_insert_edge);
  HPX_REGISTER_ACTION(_put_edge_index_action, &_put_edge_index);
  HPX_REGISTER_ACTION(_put_edge_action, &_put_edge);
}
