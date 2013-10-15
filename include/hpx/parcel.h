/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_PARCEL_H_
#define LIBHPX_PARCEL_H_

#include "hpx/action.h"
#include "hpx/agas.h"

typedef struct hpx_parcel {
  unsigned int  parcel_id;    /*!< the parcel idenitifer. */
  hpx_action_t  action;       /*!< handle to the associated action. */
  hpx_addr_t    dest;         /*!< destination locality. */
  int           flags;        /*!< flags related to the parcel. */
  size_t        payload_size;
  void         *payload;
} hpx_parcel_t;

hpx_error_t hpx_parcel_init(void);
void hpx_parcel_fini(void);

/*
 --------------------------------------------------------------------
  Parcel Handling Routines
  -------------------------------------------------------------------
*/
hpx_error_t hpx_new_parcel(hpx_action_t, void *, size_t, hpx_parcel_t *);

/* Helper to send a parcel structure */
hpx_error_t hpx_send_parcel(hpx_locality_t *loc, hpx_parcel_t *p);

/*
 --------------------------------------------------------------------
  Generic Parcel Invocation
 --------------------------------------------------------------------
*/
hpx_thread_t *hpx_call(hpx_locality_t *dest, hpx_action_t action, void *args, size_t len);

#endif /* LIBHPX_PARCEL_H_ */
