#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>

#include "adjacency_list.h"
#include "libpxgl.h"
#include "hpx/hpx.h"
#include "libsync/sync.h"

/// SSSP Chaotic-relaxation

extern uint64_t active_count;
extern uint64_t inactive_count;


static hpx_action_t _sssp_visit_vertex;
static hpx_action_t _sssp_update_vertex_distance;

static bool _try_update_vertex_distance(adj_list_vertex_t *vertex, uint64_t distance) {
  uint64_t prev_dist = sync_load(&vertex->distance, SYNC_RELAXED);
  uint64_t old_dist = prev_dist;
  while (distance < prev_dist) {
    old_dist = prev_dist;
    prev_dist = sync_cas_val(&vertex->distance, prev_dist, distance, SYNC_RELAXED, SYNC_RELAXED);
    if(prev_dist == old_dist) return true;
  }
  return false;
}

typedef struct {
  adj_list_t graph;
  uint64_t distance;
#ifdef GATHER_STAT
  hpx_addr_t sssp_stat;
#endif
} _sssp_visit_vertex_args_t;


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

static int _edge_traversal_count_action(uint64_t* num_edges)
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

static int _sssp_update_vertex_distance_action(_sssp_visit_vertex_args_t *const args) {

  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  // printf("Distance Action on %" PRIu64 "\n", target);

  if (_try_update_vertex_distance(vertex, args->distance)) {
    const uint64_t num_edges = vertex->num_edges;
    const uint64_t old_distance = args->distance;
    
    //increase active_count
    sync_fadd(&active_count,num_edges-1,SYNC_RELAXED);

    hpx_addr_t edges = hpx_lco_and_new(num_edges);
    for (int i = 0; i < num_edges; ++i) {
      adj_list_edge_t *e = &vertex->edge_list[i];
      args->distance = old_distance + e->weight;

      const hpx_addr_t index
          = hpx_addr_add(args->graph, e->dest * sizeof(hpx_addr_t), _index_array_block_size);

      hpx_call(index, _sssp_visit_vertex, args, sizeof(*args), edges);
    }
    
    hpx_gas_unpin(target);
    // printf("Distance Action waiting on edges on (%" PRIu64 ", %" PRIu32 ", %" PRIu32 ")\n", target.offset, target.base_id, target.block_bytes);
    hpx_lco_wait(edges);
    hpx_lco_delete(edges, HPX_NULL);

#ifdef GATHER_STAT
    hpx_call_sync(args->sssp_stat, _edge_traversal_count, &num_edges,sizeof(uint64_t),NULL,0);
    hpx_call_sync(args->sssp_stat, _useful_work_update, NULL,0,NULL,0);
#endif
  } else{
    sync_fadd(&inactive_count, -1, SYNC_RELAXED) ;
#ifdef GATHER_STAT
    hpx_call_sync(args->sssp_stat, _useless_work_update, NULL,0,NULL,0);
#endif
    hpx_gas_unpin(target);
  }

  // printf("Distance Action finished on %" PRIu64 "\n", target);

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

  // printf("Calling update distance on %" PRIu64 "\n", vertex);

  return hpx_call_sync(vertex, _sssp_update_vertex_distance, args, sizeof(*args), NULL, 0);
}


void _increment_active_count(){
  sync_fadd(&active_count, 1, SYNC_RELAXED); 
}


static void termination_detection_op(size_t *const output, const size_t *const input, const size_t size) {
  *output = *output + *input ;//== UINT64_MAX ? 0 : *input % MODUL) % MODUL;

}

static void termination_detection_init(void *init_val, const size_t init_val_size) {
  *(uint64_t*)init_val = 0;
}


static hpx_action_t _set_termination_lco;
static int _set_termination_lco_action(const hpx_addr_t *const args){
  uint64_t current_count;
  sync_load(&active_count, SYNC_RELAXED);
  sync_load(&inactive_count, SYNC_RELAXED);
  current_count =  active_count + inactive_count;
  hpx_lco_set(*args, sizeof(uint64_t), &current_count, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}




hpx_action_t call_sssp = 0;
int call_sssp_action(const call_sssp_args_t *const args) {
  const hpx_addr_t index
    = hpx_addr_add(args->graph, args->source * sizeof(hpx_addr_t), _index_array_block_size);

  _sssp_visit_vertex_args_t sssp_args = { .graph = args->graph, .distance = 0 };

#ifdef GATHER_STAT
  sssp_args.sssp_stat = args->sssp_stat;
#endif // GATHER_STAT

  printf("Starting SSSP on the source\n");
 
  //increment active count 
  _increment_active_count();
  
  //start the algorithm from source once
  hpx_call(index, _sssp_visit_vertex, &sssp_args, sizeof(sssp_args), HPX_NULL);
   
  int terminate_now = 1;
  while(true){
    hpx_addr_t termination_count_lco = hpx_lco_allreduce_new( HPX_LOCALITIES, 1, sizeof(uint64_t), (hpx_commutative_associative_op_t) termination_detection_op, termination_detection_init);
    //Invoke the lco on all localities
    hpx_bcast(_set_termination_lco, &termination_count_lco, sizeof(termination_count_lco), HPX_NULL);

    uint64_t terminate = 0;
    hpx_lco_get(termination_count_lco, sizeof(terminate), &terminate);
    hpx_lco_delete(termination_count_lco, HPX_NULL);
    if(terminate == 0){
      //recheck
      hpx_addr_t termination_count_recheck_lco = hpx_lco_allreduce_new( HPX_LOCALITIES, 1, sizeof(uint64_t), (hpx_commutative_associative_op_t) termination_detection_op, termination_detection_init);
      hpx_bcast(_set_termination_lco, &termination_count_recheck_lco, sizeof(termination_count_recheck_lco), HPX_NULL);

      uint64_t terminate_recheck = 0;
      hpx_lco_get(termination_count_recheck_lco, sizeof(terminate_recheck), &terminate_recheck);
      hpx_lco_delete(termination_count_recheck_lco, HPX_NULL);
      if(terminate_recheck == 0){
	terminate_now = 0;
      }
    }
    if(terminate_now == 0)
      break;
  }
  printf("Finished algorithm\n");
  return HPX_SUCCESS;


}


static __attribute__((constructor)) void _sssp_register_actions() {
  call_sssp                    = HPX_REGISTER_ACTION(call_sssp_action);
  _sssp_visit_vertex           = HPX_REGISTER_ACTION(_sssp_visit_vertex_action);
  _sssp_update_vertex_distance = HPX_REGISTER_ACTION(_sssp_update_vertex_distance_action);
 _set_termination_lco          = HPX_REGISTER_ACTION(_set_termination_lco_action);

#ifdef GATHER_STAT
  _useful_work_update          = HPX_REGISTER_ACTION(_useful_work_update_action);
 _useless_work_update          = HPX_REGISTER_ACTION(_useless_work_update_action);
 _edge_traversal_count         = HPX_REGISTER_ACTION(_edge_traversal_count_action);
#endif // GATHER_STAT
}
