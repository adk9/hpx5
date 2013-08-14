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

#include <search.h>

#include "hpx/action.h"
#include "hpx/agas.h"

extern struct hsearch_data action_table; /* TODO: move this out of globals */

typedef struct hpx_parcel_t {
  unsigned int  parcel_id;    /*!< the parcel idenitifer. */
  hpx_action_t  action;       /*!< handle to the associated action. */
  hpx_addr_t    dest;         /*!< destination locality. */
  /// ADK: I am not entirely convinced that we need these yet.
  /// hpx_action_t continuation; /*!< the continuation action. */
  /// hpx_addr_t  cdest;      /*!< target to execute continuation at. */
  int           flags;        /*!< flags related to the parcel. */
  size_t       payload_size;
  void         *payload;
} hpx_parcel_t;

int hpx_parcel_init(void);
void hpx_parcel_fini(void);

/*
 --------------------------------------------------------------------
  Parcel Handling Routines
  -------------------------------------------------------------------
*/
int hpx_new_parcel(char *, void *, size_t, hpx_parcel_t *);

/* Helper to send a parcel structure */
int hpx_send_parcel(hpx_locality_t * loc, hpx_parcel_t *p);

/**
   Helper function for sending; combines parcel plus it's payload into
   blob. Size of blob is 
   sizeof(hpx_parcel_t) + (strlen(action->name) + 1) + p->payload_size . */
int hpx_serialize_parcel(hpx_parcel_t *p, char** blob);

/*
 --------------------------------------------------------------------
  Generic Parcel Invocation
 --------------------------------------------------------------------
*/
hpx_thread_t *hpx_call(hpx_locality_t *dest, char *action, void *args, size_t len);

#endif /* LIBHPX_PARCEL_H_ */
