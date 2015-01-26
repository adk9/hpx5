#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>

#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "libpxgl/adjacency_list.h"
#include "libpxgl/sssp_dc.h"
#include "libpxgl/sssp_common.h"
#include "libpxgl/pqueue_wrapper.h"
#include "libpxgl/sssp_queue.h"
#include "libpxgl/termination.h"

/// SSSP Distributed Control

static sssp_queue_t **sssp_queues;
static size_t num_pq;
static size_t num_elem;
static size_t freq;

static hpx_action_t _handle_queue = 0;
typedef struct {
  sssp_queue_t *queue;
  adj_list_t graph;
} _handle_queue_args_t;

hpx_action_t _sssp_dc_process_vertex = 0;
int _sssp_dc_process_vertex_action(_sssp_visit_vertex_args_t *const args) {
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  // printf("Distance Action on %" SSSP_UINT_PRI "\n", target);

  if (_try_update_vertex_distance(vertex, args->distance)) {
    sssp_queue_t *queue = sssp_queues[rand() % num_pq];
    if(sssp_queue_push(queue, target, args->distance)) {
      const _handle_queue_args_t handle_args = { .queue = queue, .graph = args->graph };
      hpx_call(HPX_HERE, _handle_queue, &handle_args, sizeof(handle_args), HPX_NULL);
    }
  } else if (_get_termination() == COUNT_TERMINATION) {
    _increment_finished_count();
  } else {
    // We only support count termination with DC at this time.
    hpx_abort();
  }

  hpx_gas_unpin(target);

  return HPX_SUCCESS;
}

hpx_action_t sssp_init_dc = 0;
static int sssp_init_dc_action(sssp_init_dc_args_t *args) {
  num_pq = args->num_pq;
  // printf("num_pq: %zu\n", num_pq);
  freq = args->freq;
  // printf("freq: %zu\n", freq);  
  num_elem = args->num_elem;
  // printf("num_elem: %zu\n", num_elem);

  return HPX_SUCCESS;
}

void _sssp_init_dc_function(size_t num_pq, size_t freq) {
  hpx_addr_t init_lco = hpx_lco_future_new(0);
  const sssp_init_dc_args_t init_args = { .num_pq = num_pq, .freq = freq };
  hpx_bcast(sssp_init_dc, &init_args, sizeof(init_args), init_lco);
  hpx_lco_wait(init_lco);
  hpx_lco_delete(init_lco, HPX_NULL);
}

hpx_action_t _sssp_delete_queues = 0;
typedef struct {
  hpx_addr_t termination_lco;
  sssp_queue_t **queues;
} _sssp_delete_queues_args_t;
int _sssp_delete_queues_action(const _sssp_delete_queues_args_t * const args) {
  hpx_lco_wait(args->termination_lco);

  // printf("Delete queues wait succeeded at %zu. Deleting queues.\n", args->termination_lco);

  for(unsigned int i = 0; i < num_pq; ++i) {
    sssp_queue_free(args->queues[i]);
  }
  free(args->queues);

  if(HPX_LOCALITY_ID == 0) {
    //    hpx_lco_delete(args->termination_lco, HPX_NULL);
  }

  return HPX_SUCCESS;
}

hpx_action_t _sssp_init_queues = 0;
int _sssp_init_queues_action(const hpx_addr_t * const termination_lco) {
  // printf("num_pq in init queues: %zu\n", num_pq);
  // printf("sssp_queues before malloc: %" PRIxPTR "\n", (uintptr_t)sssp_queues);
  sssp_queues = malloc(sizeof(sssp_queue_t) * num_pq);
  // printf("sssp_queues after malloc: %" PRIxPTR "\n", (uintptr_t)sssp_queues);
  for(unsigned int i = 0; i < num_pq; ++i) {
    sssp_queues[i] = sssp_queue_create(num_elem);
  }
  const _sssp_delete_queues_args_t delete_args = { .termination_lco = *termination_lco, .queues = sssp_queues };
  hpx_call(HPX_HERE, _sssp_delete_queues, &delete_args, sizeof(delete_args), HPX_NULL);
  return HPX_SUCCESS;
}

static int _handle_queue_action(const _handle_queue_args_t * const args) {
  // printf("In the handle queue action....\n");
  uint64_t processed = 0;
  // Because we don't have a try_wait for LCOs, we kill handle queue every time a queue gets empty and restart it when we insert something.  
  hpx_addr_t v;
  distance_t d;
  adj_list_vertex_t *vertex;
  while (sssp_queue_pop(args->queue, &v, &d)) {
    //printf("Trying to pop one vertex from the priority queue in the handle queue action\n");
    // We are not handling AGAS currently and assume the vertex is on the same locality.
    // For AGAS we could just send an action.
    if (!hpx_gas_try_pin(v, (void**)&vertex)) hpx_abort();
    const distance_t current_d = sync_load(&vertex->distance, SYNC_RELAXED);
    if (current_d == d) {
      _send_update_to_neighbors(args->graph, vertex, d);
    } else {
      /* stale */
    }
    _increment_finished_count();
    hpx_gas_unpin(v);
    if (++processed == freq) {
      processed = 0;
      hpx_thread_yield();
    }
  }
  // printf("In handle queue action with empty priority queue\n");
  return HPX_SUCCESS;
}

static __attribute__((constructor)) void _sssp_register_actions() {
  HPX_REGISTER_ACTION(&_sssp_dc_process_vertex, _sssp_dc_process_vertex_action);
  HPX_REGISTER_ACTION(&sssp_init_dc, sssp_init_dc_action);
  HPX_REGISTER_ACTION(&_sssp_delete_queues, _sssp_delete_queues_action);
  HPX_REGISTER_ACTION(&_handle_queue, _handle_queue_action);
  HPX_REGISTER_ACTION(&_sssp_init_queues, _sssp_init_queues_action);
  num_pq = 0;
  sssp_queues = NULL;
}
