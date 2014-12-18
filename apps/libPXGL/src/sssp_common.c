#include "hpx/hpx.h"
#include "libpxgl/sssp_common.h"
#include "libpxgl/sssp_dc.h"
#include "libpxgl/sssp_delta.h"
#include "libpxgl/sssp_chaotic.h"
#include "libpxgl/termination.h"
#include "pxgl/pxgl.h"
#include "libsync/sync.h"
#include <stdio.h>

static hpx_action_t _sssp_process_vertex = 0;
hpx_action_t _sssp_visit_vertex = 0;

static hpx_action_t _sssp_visit_source = 0;
static hpx_action_t _sssp_print_source_adj_list = 0;
sssp_kind_t _sssp_kind = CHAOTIC_SSSP_KIND;

typedef int (*send_vertex_t)(hpx_addr_t, hpx_action_t, const void*, size_t, hpx_addr_t);
static send_vertex_t send_vertex = hpx_call;

bool _try_update_vertex_distance(adj_list_vertex_t *vertex, distance_t distance) {
  // printf("try update, vertex: %zu, distance: %zu\n", vertex, distance);
  distance_t prev_dist = sync_load(&vertex->distance, SYNC_RELAXED);
  distance_t old_dist = prev_dist;
  while (distance < prev_dist) {
    old_dist = prev_dist;
    prev_dist = sync_cas_val(&vertex->distance, prev_dist, distance, SYNC_RELAXED, SYNC_RELAXED);
    if(prev_dist == old_dist) return true;
    // sync_pause;
  }
  return false;
}

void _send_update_to_neighbors(adj_list_t graph, adj_list_vertex_t *vertex, distance_t distance) {
  const sssp_uint_t num_edges = vertex->num_edges;
  const distance_t old_distance = distance;

  // increase active_count
  if (_get_termination() == COUNT_TERMINATION) _increment_active_count(num_edges);
  
  hpx_addr_t edges = HPX_NULL;
  if (_get_termination() == AND_LCO_TERMINATION)
    edges = hpx_lco_and_new(num_edges);

  for (int i = 0; i < num_edges; ++i) {
    adj_list_edge_t *e = &vertex->edge_list[i];
    distance = old_distance + e->weight;

    // TBD: Add a check to stop sending if a better distance comes along

    const hpx_addr_t index
      = hpx_addr_add(graph, e->dest * sizeof(hpx_addr_t), _index_array_block_size);
    
    const _sssp_visit_vertex_args_t visit_args = { .graph = graph, .distance = distance };
    // printf("Calling send_vertex with vertex: %zu and distance: %zu\n", index, distance);
    send_vertex(index, _sssp_visit_vertex, &visit_args, sizeof(visit_args), 
	     _get_termination() == AND_LCO_TERMINATION ? edges : HPX_NULL);
  }
  
  // printf("Distance Action waiting on edges on (%" SSSP_UINT_PRI ", %" PRIu32 ", %" PRIu32 ")\n", target.offset, target.base_id, target.block_bytes);
  if (_get_termination() == AND_LCO_TERMINATION) {
    hpx_lco_wait(edges);
    hpx_lco_delete(edges, HPX_NULL);
  }
}

int _sssp_visit_vertex_action(const _sssp_visit_vertex_args_t *const args) {
  const hpx_addr_t target = hpx_thread_current_target();

  // printf("visit_vertex at %zu with distance %" SSSP_UINT_PRI"\n", target, args->distance);

  hpx_addr_t vertex;
  hpx_addr_t *v;
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  // printf("Calling update distance on %" SSSP_UINT_PRI "\n", vertex);

  if (_get_termination() == AND_LCO_TERMINATION) {
    return hpx_call_sync(vertex, _sssp_process_vertex, args, sizeof(*args), NULL, 0);
  } else {
    return hpx_call(vertex, _sssp_process_vertex, args, sizeof(*args), HPX_NULL);
  }
}

static int _sssp_print_source_adj_list_action(_sssp_visit_vertex_args_t *const args) {
  const hpx_addr_t target = hpx_thread_current_target();
  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;
  const SSSP_UINT_T num_edges = vertex->num_edges;
 
  printf("Printing source stat\n-------------------------------\n");
  if(num_edges>0){
    for (int i = 0; i < num_edges; ++i) {
      adj_list_edge_t *e = &vertex->edge_list[i];
      printf("dest %" PRIu64 " weight %" PRIu64 "\n", e->dest, e->weight);
    }
  }
  else{
    printf("Source does not have any neighbours\n");
  }
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

static int _sssp_visit_source_action(const _sssp_visit_vertex_args_t *const args) {
  const hpx_addr_t target = hpx_thread_current_target();
  
  hpx_addr_t vertex;
  hpx_addr_t *v;
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;
  vertex = *v;
  hpx_gas_unpin(target);
  printf("printing adjacency list for source %" SSSP_UINT_PRI "\n", vertex);

  return hpx_call_sync(vertex, _sssp_print_source_adj_list, args, sizeof(*args), NULL, 0);
}


hpx_action_t call_sssp = 0;
int call_sssp_action(const call_sssp_args_t *const args) {
  const hpx_addr_t index
    = hpx_addr_add(args->graph, args->source * sizeof(hpx_addr_t), _index_array_block_size);
  _sssp_visit_vertex_args_t sssp_args = { .graph = args->graph, .distance = 0 };
  printf("Calling sssp visit source action\n");
  //hpx_call_sync(index, _sssp_visit_source, &sssp_args, sizeof(sssp_args), HPX_NULL,0);

  // DC is only supported with count termination now.
  assert(_sssp_kind != DC_SSSP_KIND || _get_termination() == COUNT_TERMINATION);

  // Initialize counts and increment the active count for the first vertex.
  // We expect that the termination global variable is set correctly
  // on this locality.  This is true because call_sssp_action is called
  // with HPX_HERE and termination is set before the call by program flags.

  if (_get_termination() == COUNT_TERMINATION) {
    hpx_addr_t internal_termination_lco = hpx_lco_future_new(0);

    // Initialize DC if necessary
    hpx_addr_t init_queues_lco = HPX_NULL;
    if(_sssp_kind == DC_SSSP_KIND) {
      init_queues_lco = hpx_lco_future_new(0);
      hpx_bcast(_sssp_init_queues, &internal_termination_lco, sizeof(internal_termination_lco), init_queues_lco);
    }
    hpx_addr_t init_termination_count_lco = hpx_lco_future_new(0);
    // printf("Starting initialization bcast.\n");
    hpx_bcast(_initialize_termination_detection, NULL, 0, init_termination_count_lco);
    // printf("Waiting on bcast.\n");
    hpx_lco_wait(init_termination_count_lco);
    hpx_lco_delete(init_termination_count_lco, HPX_NULL);
    if(_sssp_kind == DC_SSSP_KIND) {
      hpx_lco_wait(init_queues_lco);
      hpx_lco_delete(init_queues_lco, HPX_NULL);
    }
    _increment_active_count(1);
    hpx_call(index, _sssp_visit_vertex, &sssp_args, sizeof(sssp_args), HPX_NULL);
    // printf("starting termination detection\n");
    _detect_termination(args->termination_lco, internal_termination_lco);
  } else if (_get_termination() == PROCESS_TERMINATION) {
    hpx_addr_t process = hpx_process_new(args->termination_lco);
    hpx_addr_t termination_lco = hpx_lco_future_new(0);
    hpx_process_call(process, index, _sssp_visit_vertex, &sssp_args, sizeof(sssp_args), HPX_NULL);
    hpx_lco_wait(termination_lco);
    hpx_lco_delete(termination_lco, HPX_NULL);
    hpx_process_delete(process, HPX_NULL);
    hpx_lco_set(args->termination_lco, 0, NULL, HPX_NULL, HPX_NULL);
  } else if (_get_termination() == AND_LCO_TERMINATION) {
    // printf("Calling first visit vertex.\n");
    // start the algorithm from source once
    hpx_call(index, _sssp_visit_vertex, &sssp_args, sizeof(sssp_args), args->termination_lco);
  } else {
    fprintf(stderr, "sssp: invalid termination mode.\n");
    hpx_abort();
  }

  return HPX_SUCCESS;
}

hpx_action_t initialize_sssp_kind = 0;
int initialize_sssp_kind_action(sssp_kind_t *arg) {
  _sssp_kind = *arg;
  if (_sssp_kind == CHAOTIC_SSSP_KIND) {
    _sssp_process_vertex = _sssp_chaotic_process_vertex;
  } else {
    _sssp_process_vertex = _sssp_dc_process_vertex;
  }
  return HPX_SUCCESS;
}

hpx_action_t sssp_run_delta_stepping = 0;
int sssp_run_delta_stepping_action(const void * const args) {
  send_vertex = (send_vertex_t)_delta_sssp_send_vertex;
  return HPX_SUCCESS;
}

static HPX_CONSTRUCTOR void _sssp_register_actions() {
  HPX_REGISTER_ACTION(&_sssp_visit_vertex,
                      _sssp_visit_vertex_action);
  HPX_REGISTER_ACTION(&call_sssp,
                      call_sssp_action);
  HPX_REGISTER_ACTION(&initialize_sssp_kind,
                      initialize_sssp_kind_action);
  HPX_REGISTER_ACTION(&sssp_run_delta_stepping,
		      sssp_run_delta_stepping_action);
  HPX_REGISTER_ACTION(&_sssp_visit_source, _sssp_visit_source_action);
  HPX_REGISTER_ACTION(&_sssp_print_source_adj_list,_sssp_print_source_adj_list_action);
}
