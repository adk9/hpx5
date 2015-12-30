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

#ifndef LIBHPX_LIBHPX_H
#define LIBHPX_LIBHPX_H

#include <errno.h>
#include <hpx/attributes.h>
#include <libhpx/config.h>
#include <hpx/hpx.h>

enum {
  LIBHPX_ENOMEM = -(ENOMEM),
  LIBHPX_EINVAL = -(EINVAL),
  LIBHPX_ERROR = -2,
  LIBHPX_EUNIMPLEMENTED = -1,
  LIBHPX_OK = 0,
  LIBHPX_RETRY
};

struct config;
typedef struct config libhpx_config_t;
libhpx_config_t *libhpx_get_config(void) HPX_PUBLIC;

/// Print the version of libhpx.
void libhpx_print_version(void) HPX_PUBLIC;

void hpx_send_mail(int id, hpx_parcel_t *p);
void hpx_parcel_create_and_send_mail(hpx_action_t action, int data,
					int victim_thread_id);
void hpx_parcel_stack_push(hpx_parcel_t **stack, hpx_parcel_t *p);
/// set the plugin scheduler
void hpx_set_priority_scheduler(hpx_parcel_t* (*work_produce)(),
				 void (*work_consume)(hpx_parcel_t*),
				 hpx_parcel_t* (*work_steal)()) HPX_PUBLIC;
hpx_parcel_t *hpx_create_new_parcel(hpx_addr_t target, hpx_action_t action,
				      hpx_addr_t c_target, hpx_action_t c_action,
				      hpx_pid_t pid, const void *data, size_t len);
int  hpx_check_no_thread();
#endif  // LIBHPX_LIBHPX_H
