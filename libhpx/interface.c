// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <hpx/hpx.h>
#include <libhpx/libhpx.h>
#include <libhpx/worker.h>
#include <libhpx/scheduler.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include <libhpx/gas.h>
#include <assert.h>

/* void hpx_turnon_network_stat(bool turnon) { */
/*   for (int i = 0, e = here->sched->n_workers; i < e;   i) { */
/*     worker_t *w = scheduler_get_worker(here->sched, i); */
/*     w->stats.turnon_network_stat = turnon; */
/*   } */
/* } */

int hpx_gas_owner_of(hpx_addr_t addr) {
  return gas_owner_of(here->gas, addr);
}

hpx_parcel_t *hpx_create_new_parcel(hpx_addr_t target, hpx_action_t action,
				    hpx_addr_t c_target, hpx_action_t c_action,
				    hpx_pid_t pid, const void *data, size_t len) {
  hpx_parcel_t *p = parcel_new(target, action, c_target, c_action, pid, data, len);
  parcel_prepare(p);
  return p;
}

void hpx_send_mail(int id, hpx_parcel_t *p) {
  //printf("Sending mail from thread %d\n", id);
  //printf("%d\n", p->buffer[0]);
  worker_t *worker = scheduler_get_worker(here->sched, id);
  //parcel_prepare(p);
  //assert(*((int*) p->buffer) <= HPX_THREADS);
  _send_mail(p, worker);
}

void hpx_parcel_create_and_send_mail(hpx_action_t action, int data,
				      int victim_thread_id) {
  hpx_addr_t target = HPX_HERE;
  hpx_pid_t pid = hpx_thread_current_pid();
  int payload = data;
  //printf("payload contain id %d\n", payload);
  hpx_parcel_t *p = parcel_new(target, action, 0, 0, pid, &payload, sizeof(int));
  parcel_prepare(p);
  //printf("After preparing parcel sending the buffer with contents id %d\n", *((int*) p->buffer));
  assert(*((int*) p->buffer) <= HPX_THREADS);
  worker_t *victim = scheduler_get_worker(here->sched, victim_thread_id);
  _send_mail(p, victim);
  //printf("The buffer of the parcel contains id %d\n", *((int*) p->buffer));
  //printf("The buffer of the parcel contains id pointer %d\n",  (int)&payload);
  //return p;
  /*hpx_parcel_t *parcel_new(hpx_addr_t target, hpx_action_t action, hpx_addr_t c_target,
    hpx_action_t c_action, hpx_pid_t pid, const void *data,
    size_t len)*/
}

void hpx_parcel_stack_push(hpx_parcel_t **stack, hpx_parcel_t *p) {
  p->next = *stack;
  *stack = p;
}

void hpx_set_priority_scheduler(hpx_parcel_t* (*work_produce)(),
				void (*work_consume)(hpx_parcel_t*),
				hpx_parcel_t* (*work_steal)()) {
  here->sched->p_sched.work_produce = work_produce;
  here->sched->p_sched.work_consume = work_consume;
  here->sched->p_sched.work_steal   = work_steal;
  here->sched->p_sched.on           = true;
}

int  hpx_check_no_thread() {
  int n = here->sched->n_workers;
  if (n == 1) {
    return 1;
  }
  else {
    return 0;
  }
}
