
#include <stdio.h>

#include "libpxgl/gteps.h"

#define MODUL ((distance_t) 1 << 62)

static void gteps_calculate_op(size_t *const output, const size_t *const input, const size_t size) {
  *output = *output + *input ;
}

static void gteps_calculate_init(void *init_val, const size_t init_val_size) {
  *(distance_t*)init_val = 0;
}

static hpx_action_t _gteps_send_dist = 0;
static int _gteps_send_dist_action(const hpx_addr_t *const args) {
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t vertex;
  adj_list_vertex_t *v;
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  if(vertex.distance==SSSP_UINT_MAX){
    const size_t edge_traversed = 0;
    hpx_lco_set(*args, sizeof(edge_traversed), &edge_traversed, HPX_NULL, HPX_NULL);
  }
  else  
    hpx_lco_set(*args, sizeof(vertex.num_edges), &vertex.num_edges, HPX_NULL, HPX_NULL);

  return HPX_SUCCESS;
}

static hpx_action_t _gteps_visit_vertex = 0;
static int _gteps_visit_vertex_action(const hpx_addr_t *const args) {
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t vertex;
  hpx_addr_t *v;
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  return hpx_call(vertex, _gteps_send_dist, args, sizeof(*args), HPX_NULL);
}

hpx_action_t gteps_calculate = 0;
int gteps_calculate_action(const sssp_uint_t *const num_vertices) {
  const hpx_addr_t adj_list = hpx_thread_current_target();
  hpx_addr_t calculate_lco = hpx_lco_allreduce_new(*num_vertices, 1, sizeof(size_t), (hpx_commutative_associative_op_t) gteps_calculate_op, gteps_calculate_init);

  // since we know how we have allocated the index block array, we
  // compute the block size here from the number of vertices.
  uint32_t block_size = ((*num_vertices + HPX_LOCALITIES - 1) / HPX_LOCALITIES) * sizeof(hpx_addr_t);
  int i;
  for (i = 0; i < *num_vertices; ++i) {
    const hpx_addr_t vertex_index = hpx_addr_add(adj_list, i * sizeof(hpx_addr_t), block_size);
    hpx_call(vertex_index, _gteps_visit_vertex, &calculate_lco, sizeof(calculate_lco), HPX_NULL);
  }

  // printf("Finished with the loop\n");

  hpx_thread_continue(sizeof(calculate_lco), &calculate_lco);

  return HPX_SUCCESS;
}

static __attribute__((constructor)) void _gteps_register_actions() {
  HPX_REGISTER_ACTION(&gteps_calculate, gteps_calculate_action);
  HPX_REGISTER_ACTION(&_gteps_visit_vertex, _gteps_visit_vertex_action);
  HPX_REGISTER_ACTION(&_gteps_send_dist, _gteps_send_dist_action);
}
