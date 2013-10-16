/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Handler Function Definitions
  hpx_parcelhandler.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Benjamin D. Martin <benjmart [at] indiana.edu>
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_PARCEL_PARCELHANDLER_H_
#define LIBHPX_PARCEL_PARCELHANDLER_H_

/*
 --------------------------------------------------------------------
  Parcel Handler Data

  ctx                          Pointer to the thread context
  thread                       Pointer to the parcel handler's thread
  fut                          Pointer to the parcel handler's return
                               future
 --------------------------------------------------------------------
*/
struct hpx_context;
struct hpx_thread;
struct hpx_future;

typedef struct {
  struct hpx_context *ctx;
  struct hpx_thread *thread;  
  struct hpx_future *fut;
} hpx_parcelhandler_t;

extern hpx_parcelhandler_t *__hpx_parcelhandler;

/*
 --------------------------------------------------------------------
  Parcel Handler Functions
 --------------------------------------------------------------------
*/

/**
  Create the parcel handler. Returns a newly allocated and initialized
  parcel handler. 

  *Warning*: In the present implementation, there should only ever be
  one parcel handler, or the system will break. 
 */
hpx_parcelhandler_t *hpx_parcelhandler_create(struct hpx_context *);

/**
   Clean up the parcel handler: Destroy any outstanding threads in a
   clean way, free any related data structures, and free the parcel
   handler itself.
 */
void hpx_parcelhandler_destroy(hpx_parcelhandler_t *);

#endif /* LIBHPX_PARCEL_PARCELHANDLER_H_ */
