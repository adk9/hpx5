
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>

#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "libpxgl/adjacency_list.h"
#include "libpxgl/sssp.h"

/// SSSP Chaotic-relaxation

/// Termination detection counts
static SSSP_UINT_T active_count;
static SSSP_UINT_T finished_count;
/// Termination detection choice
termination_t termination;

static void _increment_active_count(SSSP_UINT_T n) {
  sync_fadd(&active_count, n, SYNC_RELEASE);
}

static void _increment_finished_count() {
  sync_fadd(&finished_count, 1, SYNC_RELEASE);
}

static termination_t get_termination() {
  return sync_load(&termination, SYNC_CONSUME);
}

static void _termination_detection_op(SSSP_UINT_T *const output, const SSSP_UINT_T *const input, const size_t size) {
  output[0] = output[0] + input[0];
  output[1] = output[1] + input[1];
}

static void _termination_detection_init(void *init_val, const size_t init_val_size) {
  ((SSSP_UINT_T*)init_val)[0] = 0;
  ((SSSP_UINT_T*)init_val)[1] = 0;  
}

typedef struct {
  adj_list_t graph;
  SSSP_UINT_T distance;
#ifdef GATHER_STAT
  hpx_addr_t sssp_stat;
#endif
} _sssp_visit_vertex_args_t;

static hpx_action_t _initialize_termination_detection = 0;
static hpx_action_t _sssp_update_vertex_distance = 0;
static hpx_action_t _sssp_visit_vertex = 0;
static hpx_action_t _send_termination_count = 0;
hpx_action_t call_sssp = 0;

#ifdef GATHER_STAT
static hpx_action_t _useful_work_update;
static hpx_action_t _useless_work_update;
static hpx_action_t _edge_traversal_count;

static int _useful_work_update_action()
{
  const hpx_addr_t target = hpx_thread_current_target();
  _sssp_statistics *sssp_stat;
  if (!hpx_gas_try_pin(target, (void**)&sssp_stat))
    return HPX_RESEND;

  sync_fadd(&(sssp_stat->useful_work), 1, SYNC_RELAXED);
  hpx_gas_unpin(target);

  return HPX_SUCCESS;
}

static int _useless_work_update_action()
{
  const hpx_addr_t target = hpx_thread_current_target();
   _sssp_statistics *sssp_stat;
  if (!hpx_gas_try_pin(target, (void**)&sssp_stat))
    return HPX_RESEND;

  sync_fadd(&(sssp_stat->useless_work), 1, SYNC_RELAXED);
  hpx_gas_unpin(target);

  return HPX_SUCCESS;
}

static int _edge_traversal_count_action(SSSP_UINT_T* num_edges)
{
  const hpx_addr_t target = hpx_thread_current_target();
  _sssp_statistics *sssp_stat;
  if (!hpx_gas_try_pin(target, (void**)&sssp_stat))
    return HPX_RESEND;
  sync_fadd(&(sssp_stat->edge_traversal_count), *num_edges, SYNC_RELAXED);
  hpx_gas_unpin(target);

  return HPX_SUCCESS;
}
#endif // GATHER_STAT

static int _initialize_termination_detection_action(void *arg) {
  // printf("Initializing termination detection.\n");
  sync_store(&active_count, 0, SYNC_SEQ_CST);
  sync_store(&finished_count, 0, SYNC_SEQ_CST);
  sync_store(&termination, COUNT_TERMINATION, SYNC_SEQ_CST);
  return HPX_SUCCESS;
}

static bool _try_update_vertex_distance(adj_list_vertex_t *vertex, SSSP_UINT_T distance) {
  SSSP_UINT_T prev_dist = sync_load(&vertex->distance, SYNC_RELAXED);
  SSSP_UINT_T old_dist = prev_dist;
  while (distance < prev_dist) {
    old_dist = prev_dist;
    prev_dist = sync_cas_val(&vertex->distance, prev_dist, distance, SYNC_RELAXED, SYNC_RELAXED);
    if(prev_dist == old_dist) return true;
    // sync_pause;
  }
  return false;
}

static int _sssp_update_vertex_distance_action(_sssp_visit_vertex_args_t *const args) {
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  // printf("Distance Action on %" SSSP_UINT_PRI "\n", target);

  if (_try_update_vertex_distance(vertex, args->distance)) {
    const SSSP_UINT_T num_edges = vertex->num_edges;
    const SSSP_UINT_T old_distance = args->distance;

    // increase active_count
    if (get_termination() == COUNT_TERMINATION) _increment_active_count(num_edges);

    hpx_addr_t edges = HPX_NULL;
    if (get_termination() == AND_LCO_TERMINATION)
      edges = hpx_lco_and_new(num_edges);

    for (int i = 0; i < num_edges; ++i) {
      adj_list_edge_t *e = &vertex->edge_list[i];
      args->distance = old_distance + e->weight;

      const hpx_addr_t index
          = hpx_addr_add(args->graph, e->dest * sizeof(hpx_addr_t), _index_array_block_size);

      hpx_call(index, _sssp_visit_vertex, args, sizeof(*args), 
               get_termination() == AND_LCO_TERMINATION ? edges : HPX_NULL);
    }

    hpx_gas_unpin(target);
    // printf("Distance Action waiting on edges on (%" SSSP_UINT_PRI ", %" PRIu32 ", %" PRIu32 ")\n", target.offset, target.base_id, target.block_bytes);
    if (get_termination() == AND_LCO_TERMINATION) {
      hpx_lco_wait(edges);
      hpx_lco_delete(edges, HPX_NULL);
    }

#ifdef GATHER_STAT
    hpx_call_sync(args->sssp_stat, _edge_traversal_count, &num_edges,sizeof(SSSP_UINT_T),NULL,0);
    hpx_call_sync(args->sssp_stat, _useful_work_update, NULL,0,NULL,0);
#endif
  } else {
    hpx_gas_unpin(target);
#ifdef GATHER_STAT
    hpx_call_sync(args->sssp_stat, _useless_work_update, NULL,0,NULL,0);
#endif
  }

  // Finished count increment could be hoisted up to the points where
  // the last "important" thing happens, but the code would be much
  // uglier.
  if (get_termination() == COUNT_TERMINATION) {
    _increment_finished_count();
  }

  // printf("Distance Action finished on %" SSSP_UINT_PRI "\n", target);

  return HPX_SUCCESS;
}

static int _sssp_visit_vertex_action(const _sssp_visit_vertex_args_t *const args) {
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t vertex;
  hpx_addr_t *v;
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  // printf("Calling update distance on %" SSSP_UINT_PRI "\n", vertex);

  if (termination == AND_LCO_TERMINATION) {
    return hpx_call_sync(vertex, _sssp_update_vertex_distance, args, sizeof(*args), NULL, 0);
  } else {
    return hpx_call(vertex, _sssp_update_vertex_distance, args, sizeof(*args), HPX_NULL);
  }
}

static int _send_termination_count_action(const hpx_addr_t *const args) {
  SSSP_UINT_T current_counts[2];
  // Does the order matter? We want to make sure we got all the active
  // actions last. We may make a mistake that will hold off
  // termination, but we won't terminate prematurely.
  current_counts[1] = sync_load(&finished_count, SYNC_CONSUME);
  current_counts[0] = sync_load(&active_count, SYNC_CONSUME);
  hpx_lco_set(*args, sizeof(current_counts), current_counts, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static void detect_termination(const hpx_addr_t termination_lco) {
  hpx_addr_t termination_count_lco = hpx_lco_allreduce_new(HPX_LOCALITIES, 1, 2*sizeof(sssp_int_t), (hpx_commutative_associative_op_t) _termination_detection_op, _termination_detection_init);
  enum { PHASE_1, PHASE_2 } phase = PHASE_1;
  SSSP_UINT_T last_finished_count = 0;

  while(true) {
    hpx_bcast(_send_termination_count, &termination_count_lco, sizeof(termination_count_lco), HPX_NULL);
    SSSP_UINT_T activity_counts[2];
    hpx_lco_get(termination_count_lco, sizeof(activity_counts), activity_counts);
    const SSSP_UINT_T active_count = activity_counts[0];
    const SSSP_UINT_T finished_count = activity_counts[1];
    sssp_int_t activity_count = active_count - finished_count;
    // printf("activity_count: %" SSSPI_INT_PRI ", phase: %d\n", activity_count, phase);
    if (activity_count != 0) {
      phase = PHASE_1;
      continue;
    } else if(phase == PHASE_2 && last_finished_count == finished_count) {
      hpx_lco_set(termination_lco, 0, NULL, HPX_NULL, HPX_NULL);
      break;
    } else {
      phase = PHASE_2;
      last_finished_count = finished_count;
    }
  }

  hpx_lco_delete(termination_count_lco, HPX_NULL);
}

int call_sssp_action(const call_sssp_args_t *const args) {
  const hpx_addr_t index
    = hpx_addr_add(args->graph, args->source * sizeof(hpx_addr_t), _index_array_block_size);
  _sssp_visit_vertex_args_t sssp_args = { .graph = args->graph, .distance = 0 };

#ifdef GATHER_STAT
  sssp_args.sssp_stat = args->sssp_stat;
#endif // GATHER_STAT

  // Initialize counts and increment the active count for the first vertex.
  // We expect that the termination global variable is set correctly
  // on this locality.  This is true because call_sssp_action is called
  // with HPX_HERE and termination is set before the call by program flags.

  if (get_termination() == COUNT_TERMINATION) {
    hpx_addr_t init_termination_count_lco = hpx_lco_future_new(0);
    // printf("Starting initialization bcast.\n");
    hpx_bcast(_initialize_termination_detection, NULL, 0, init_termination_count_lco);
    // printf("Waiting on bcast.\n");
    hpx_lco_wait(init_termination_count_lco);
    hpx_lco_delete(init_termination_count_lco, HPX_NULL);
    _increment_active_count(1);

    hpx_call(index, _sssp_visit_vertex, &sssp_args, sizeof(sssp_args), HPX_NULL);
    // printf("starting termination detection\n");
    detect_termination(args->termination_lco);

  } else if (get_termination() == PROCESS_TERMINATION) {
    hpx_addr_t process = hpx_process_new(args->termination_lco);
    hpx_addr_t termination_lco = hpx_lco_future_new(0);
    hpx_process_call(process, index, _sssp_visit_vertex, &sssp_args, sizeof(sssp_args), HPX_NULL);
    hpx_lco_wait(termination_lco);
    hpx_lco_delete(termination_lco, HPX_NULL);
    hpx_process_delete(process, HPX_NULL);
    hpx_lco_set(args->termination_lco, 0, NULL, HPX_NULL, HPX_NULL);

  } else if (get_termination() == AND_LCO_TERMINATION) {
    // printf("Calling first visit vertex.\n");
    // start the algorithm from source once
    hpx_call(index, _sssp_visit_vertex, &sssp_args, sizeof(sssp_args), args->termination_lco);

  } else {
    fprintf(stderr, "sssp: invalid termination mode.\n");
    hpx_abort();
  }

  return HPX_SUCCESS;
}

static __attribute__((constructor)) void _sssp_register_actions() {
  HPX_REGISTER_ACTION(&call_sssp, call_sssp_action);
  HPX_REGISTER_ACTION(&_sssp_visit_vertex, _sssp_visit_vertex_action);
  HPX_REGISTER_ACTION(&_sssp_update_vertex_distance,
                      _sssp_update_vertex_distance_action);
  HPX_REGISTER_ACTION(&_send_termination_count,
                      _send_termination_count_action);
  HPX_REGISTER_ACTION(&_initialize_termination_detection,
                      _initialize_termination_detection_action);
  termination                  = COUNT_TERMINATION;

#ifdef GATHER_STAT
  HPX_REGISTER_ACTION(&_useful_work_update, _useful_work_update_action);
  HPX_REGISTER_ACTION(&_useless_work_update, _useless_work_update_action);
  HPX_REGISTER_ACTION(&_edge_traversal_count, _edge_traversal_count_action);
#endif // GATHER_STAT
}
