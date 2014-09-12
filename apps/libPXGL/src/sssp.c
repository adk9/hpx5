
#include <string.h>
#include <assert.h>

#include "adjacency_list.h"
#include "libpxgl.h"
#include "hpx/hpx.h"
#include "libsync/sync.h"


/// SSSP Chaotic-relaxation


static bool _try_update_vertex_distance(adj_list_vertex_t *vertex, int distance) {
  uint64_t prev_dist = sync_load(&vertex->distance, SYNC_ACQUIRE);
  if (distance < prev_dist) {
    if (!sync_cas(&vertex->distance, prev_dist, distance, SYNC_RELEASE, SYNC_RELAXED))
      return _try_update_vertex_distance(vertex, distance);
    return true;
  }
  return false;
}

typedef struct {
  adj_list_t graph;
  uint64_t distance;
} _sssp_visit_vertex_args_t;


static hpx_action_t _sssp_update_vertex_distance;
static int _sssp_update_vertex_distance_action(_sssp_visit_vertex_args_t *args) {
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  if (_try_update_vertex_distance(vertex, args->distance)) {
    uint64_t num_edges = sync_load(&vertex->num_edges, SYNC_ACQUIRE);

    uint64_t new_dist;
    hpx_addr_t edges = hpx_lco_and_new(num_edges);
    adj_list_edge_t *e;
    for (int i = 0; i < num_edges; ++i) {
      e = &vertex->edge_list[i];
      new_dist = args->distance + e->weight;

      const hpx_addr_t index
          = hpx_addr_add(args->graph, e->dest * sizeof(hpx_addr_t));

      _sssp_visit_vertex_args_t sssp_args = { .graph = args->graph, .distance = new_dist };
      hpx_call(index, _sssp_update_vertex_distance, &sssp_args, sizeof(sssp_args), edges);
    }

    hpx_gas_unpin(target);
    hpx_lco_wait(edges);
    hpx_lco_delete(edges, HPX_NULL);
  } else
    hpx_gas_unpin(target);

  return HPX_SUCCESS;
}


static hpx_action_t _sssp_visit_vertex;
static int _sssp_visit_vertex_action(_sssp_visit_vertex_args_t *args) {
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t vertex;
  hpx_addr_t *v;
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);
  return hpx_call_sync(vertex, _sssp_update_vertex_distance, args, sizeof(*args), NULL, 0);
}


hpx_action_t call_sssp;
int call_sssp_action(call_sssp_args_t *args) {
  const hpx_addr_t index
    = hpx_addr_add(args->graph, args->source * sizeof(hpx_addr_t));

  _sssp_visit_vertex_args_t sssp_args = { .graph = args->graph, .distance = 0 };
  return hpx_call_sync(index, _sssp_visit_vertex, &sssp_args, sizeof(sssp_args), NULL, 0);
}


static __attribute__((constructor)) void _sssp_register_actions() {
  call_sssp                    = HPX_REGISTER_ACTION(call_sssp_action);
  _sssp_visit_vertex           = HPX_REGISTER_ACTION(_sssp_visit_vertex_action);
  _sssp_update_vertex_distance = HPX_REGISTER_ACTION(_sssp_update_vertex_distance_action);
}
