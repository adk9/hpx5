/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Thread Function Definitions
  hpx_thread.h

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <stdlib.h>
#include <hpx.h>

hpx_action_t
hpx_action_register(const char *id, hpx_action_handler_t func) {
  return HPX_ACTION_NULL;
}

int
hpx_init(int argc, char * const argv[argc]) {
  return 0;
}

int
hpx_run(hpx_action_t f, void *args, unsigned size) {
  return 0;
}

void
hpx_shutdown(int value) {
  exit(value);
}

int
hpx_get_my_rank(void) {
  return 0;
}

int
hpx_get_num_ranks(void) {
  return 0;
}

void
hpx_thread_wait(hpx_addr_t future, void *value) {
}

void
hpx_thread_wait_all(unsigned n, hpx_addr_t futures[], void *values[]) {
}

void
hpx_thread_yield(void) {
}

int
hpx_thread_exit(void *value, unsigned size) {
  return 0;
}

hpx_addr_t
hpx_future_new(int size) {
  return HPX_NULL;
}

void
hpx_future_delete(hpx_addr_t future) {
}

void
hpx_call(int rank, hpx_action_t action, void *args, size_t len,
         hpx_addr_t result) {
}

hpx_time_t
hpx_time_now(void) {
  return 0;
}

uint64_t
hpx_time_to_us(hpx_time_t time) {
  return time;
}

uint64_t
hpx_time_to_ms(hpx_time_t time) {
  return time;
}

hpx_parcel_t *
hpx_parcel_acquire(unsigned size) {
  return NULL;
}

void
hpx_parcel_set_action(hpx_parcel_t *p, hpx_action_t action) {
}

void *
hpx_parcel_get_data(hpx_parcel_t *p) {
  return NULL;
}

void
hpx_parcel_send(int rank, hpx_parcel_t *p, hpx_addr_t thread,
                hpx_addr_t result) {
}
