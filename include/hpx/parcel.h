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

typedef struct hpx_parcel_t {
  unsigned int  parcel_id;    /*!< the parcel idenitifer. */
  hpx_action_t  action;       /*!< handle to the associated action. */
  hpx_addr_t    dest;         /*!< destination locality. */
  /// ADK: I am not entirely convinced that we need these yet.
  /// hpx_action_t continuation; /*!< the continuation action. */
  /// hpx_addr_t  cdest;      /*!< target to execute continuation at. */
  int           flags;        /*!< flags related to the parcel. */
  void         *payload;
} hpx_parcel_t;

/*
 --------------------------------------------------------------------
  Parcel Handling Routines
  -------------------------------------------------------------------
*/
int hpx_new_parcel(char *, void *, size_t, hpx_parcel_t *);


/*
 --------------------------------------------------------------------
  Generic Parcel Invocation
 --------------------------------------------------------------------
*/
hpx_thread_t *hpx_call(hpx_locality_t *dest, char *action, void *args, size_t len);

#endif /* LIBHPX_PARCEL_H_ */
