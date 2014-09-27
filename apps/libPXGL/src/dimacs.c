#include "dimacs.h"

#define MODUL ((distance_t) 1 << 62)

static void dimacs_checksum_op(distance_t *const output, const distance_t *const input, const size_t size) {
  *output = *output + (*input == UINT64_MAX ? 0 : *input % MODUL) % MODUL;
}

static void dimacs_checksum_init(void *init_val, const size_t init_val_size) {
  *(distance_t*)init_val = 0;
}

static hpx_action_t _dimacs_send_dist = 0;
static int _dimacs_send_dist_action(const hpx_addr_t *const args) {
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t vertex;
  adj_list_vertex_t *v;
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  hpx_lco_set(*args, sizeof(vertex.distance), &vertex.distance, HPX_NULL, HPX_NULL);

  return HPX_SUCCESS;
}

static hpx_action_t _dimacs_visit_vertex = 0;
static int _dimacs_visit_vertex_action(const hpx_addr_t *const args) {
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t vertex;
  hpx_addr_t *v;
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  return hpx_call(vertex, _dimacs_send_dist, args, sizeof(*args), HPX_NULL);
}

hpx_action_t dimacs_checksum = 0;
int dimacs_checksum_action(const size_t *const num_vertices) {
  const hpx_addr_t adj_list = hpx_thread_current_target();
  hpx_addr_t checksum_lco = hpx_lco_allreduce_new(*num_vertices, sizeof(distance_t), (hpx_commutative_associative_op_t) dimacs_checksum_op, dimacs_checksum_init);
  for(size_t i = 0; i < *num_vertices; ++i) {
    const hpx_addr_t vertex_index = hpx_addr_add(adj_list, i * sizeof(hpx_addr_t));
    hpx_call(vertex_index, _dimacs_visit_vertex, &vertex_index, sizeof(vertex_index), HPX_NULL);
  }

  hpx_thread_continue(sizeof(checksum_lco), &checksum_lco);

  return HPX_SUCCESS;
}

static __attribute__((constructor)) void _dimacs_register_actions() {
  dimacs_checksum = HPX_REGISTER_ACTION(dimacs_checksum_action);
  _dimacs_visit_vertex = HPX_REGISTER_ACTION(_dimacs_visit_vertex_action);
  _dimacs_send_dist = HPX_REGISTER_ACTION(_dimacs_send_dist_action);
}
