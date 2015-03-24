#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>

#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "libpxgl/termination.h"

/// Termination Detection

hpx_action_t _initialize_termination_detection = 0;
static hpx_action_t _set_termination = 0;
static hpx_action_t _send_termination_count = 0;

/// Termination detection counts
static sssp_uint_t active_count;
static sssp_uint_t finished_count;
/// Termination detection choice
termination_t _termination;

void _increment_active_count(sssp_uint_t n) {
  sync_fadd(&active_count, n, SYNC_RELAXED);
}

void _increment_finished_count() {
  sync_fadd(&finished_count, 1, SYNC_RELAXED);
}

sssp_uint_t _relaxed_get_active_count() {
  return sync_load(&active_count, SYNC_RELAXED);
}

sssp_uint_t _relaxed_get_finished_count() {
  return sync_load(&finished_count, SYNC_RELAXED);
}

termination_t _get_termination() {
  return sync_load(&_termination, SYNC_RELAXED);
}

static void _termination_detection_op(PXGL_UINT_T *const output, const PXGL_UINT_T *const input, const size_t size) {
  output[0] = output[0] + input[0];
  output[1] = output[1] + input[1];
}

static void _termination_detection_init(void *init_val, const size_t init_val_size) {
  ((PXGL_UINT_T*)init_val)[0] = 0;
  ((PXGL_UINT_T*)init_val)[1] = 0;  
}

static int _send_termination_count_action(const hpx_addr_t *const args) {
  PXGL_UINT_T current_counts[2];
  // Order all previous relaxed memory accesses.
  sync_fence(SYNC_SEQ_CST);
  current_counts[1] = sync_load(&finished_count, SYNC_RELAXED);
  current_counts[0] = sync_load(&active_count, SYNC_RELAXED);
  hpx_thread_yield();
  hpx_lco_set(*args, sizeof(current_counts), current_counts, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

void set_termination(termination_t termination) {
  hpx_addr_t set_termination_lco = hpx_lco_future_new(0);
  hpx_bcast(_set_termination, set_termination_lco, &termination,
            sizeof(termination));
  hpx_lco_wait(set_termination_lco);
  hpx_lco_delete(set_termination_lco, HPX_NULL);
  return;
}

int _set_termination_action(termination_t *arg) {
  sync_store(&_termination, COUNT_TERMINATION, SYNC_RELAXED);
  sync_fence(SYNC_SEQ_CST);
  return HPX_SUCCESS;
}

int _initialize_termination_detection_action(void *arg) {
  // printf("Initializing termination detection.\n");
  sync_store(&active_count, 0, SYNC_RELAXED);
  sync_store(&finished_count, 0, SYNC_RELAXED);
  // Order the relaxed memory operations that came before.
  sync_fence(SYNC_SEQ_CST);
  return HPX_SUCCESS;
}

void _initialize_termination() {
  hpx_bcast_sync(_initialize_termination_detection, NULL, 0);
}

void _detect_termination(const hpx_addr_t termination_lco, const hpx_addr_t internal_termination_lco) {
  hpx_addr_t termination_count_lco = hpx_lco_allreduce_new(HPX_LOCALITIES, 1, 2*sizeof(sssp_int_t), (hpx_monoid_id_t)_termination_detection_init, (hpx_monoid_op_t) _termination_detection_op);
  enum { PHASE_1, PHASE_2 } phase = PHASE_1;
  PXGL_UINT_T last_finished_count = 0;

  while(true) {
    hpx_bcast(_send_termination_count, HPX_NULL, &termination_count_lco,
              sizeof(termination_count_lco));
    sssp_uint_t activity_counts[2];
    hpx_lco_get(termination_count_lco, sizeof(activity_counts), activity_counts);
    const sssp_uint_t active_count = activity_counts[0];
    const sssp_uint_t finished_count = activity_counts[1];
    sssp_int_t activity_count = active_count - finished_count;
    //printf("activity_count: %" PXGL_INT_PRI ", active: %" PXGL_UINT_PRI ", finished %" PXGL_UINT_PRI ", phase: %d\n", activity_count, active_count, finished_count, phase);
    if (activity_count != 0) {
      phase = PHASE_1;
      continue;
    } else if(phase == PHASE_2 && last_finished_count == finished_count) {
      // printf("Setting termination LCOs: %zu %zu\n", termination_lco, internal_termination_lco);
      hpx_lco_set(termination_lco, 0, NULL, HPX_NULL, HPX_NULL);
      hpx_lco_set(internal_termination_lco, 0, NULL, HPX_NULL, HPX_NULL);
      // printf("Termination LCOs set.\n");
      // printf("Finished termination detection with active: %" PXGL_UINT_PRI ", finished %" PXGL_UINT_PRI "\n", active_count, finished_count);
      break;
    } else {
      phase = PHASE_2;
      last_finished_count = finished_count;
    }
  }
  // printf("Finished termination_detection.\n");
  hpx_lco_delete(termination_count_lco, HPX_NULL);
}

static HPX_CONSTRUCTOR void _sssp_register_actions() {
  HPX_REGISTER_ACTION(_initialize_termination_detection_action,
                      &_initialize_termination_detection);
  HPX_REGISTER_ACTION(_send_termination_count_action,
                      &_send_termination_count);
  HPX_REGISTER_ACTION(_set_termination_action,
                      &_set_termination);
  // Default termination detection algorithm
  _termination = COUNT_TERMINATION;
}
