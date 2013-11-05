/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  ParcelQueue Functions
  src/parcel/serialization.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
  Luke Dalessandro   <ldalessa [at] indiana.edu>
  ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>                             /* uint */
#include <string.h>                             /* memcpy */
#include <strings.h>                            /* bzero */

#include "hpx/action.h"                         /* hpx_action_invoke */
#include "hpx/error.h"                          /* HPX_{SUCCESS,ERROR} */
#include "hpx/future.h"                         /* future stuff */
#include "hpx/parcel.h"                         /* hpx_parcel_* */
#include "hpx/thread.h"                         /* hpx_thread_create */
#include "action.h"                             /* action_lookup */
#include "debug.h"                              /* dbg_ suff */
#include "parcel.h"                             /* struct hpx_parcel */
#include "parcelhandler.h"                      /* parcelhandler_send */
//#include "allocator.h"                          /* parcel_{get,set} */

/** Typedefs used for convenience in this source file. @{ */
typedef struct hpx_addr     addr_t;
typedef struct hpx_future   future_t;
typedef struct hpx_parcel   parcel_t;
typedef struct hpx_locality locality_t;
typedef struct hpx_thread   thread_t;
/** @} */

/** temporary get/put interfaces @{ */
static parcel_t *
parcel_get(size_t bytes)
{
  return malloc(sizeof(parcel_t) + bytes);
}

static void
parcel_put(parcel_t *parcel)
{
  dbg_assert_precondition(parcel);
  free(parcel);
}
/** @} */

/**
 * Forward the request for a parcel to the parcel allocator.
 */
parcel_t *
hpx_parcel_acquire(size_t bytes)
{
  return parcel_get(bytes);
}

/**
 * Forward the parcel release to the parcel allocator.
 *
 * The parcel_put() interface doesn't want a NULL pointer, while the
 * hpx_parcel_release() interface doesn't care. Match those internally.
 */
void
hpx_parcel_release(parcel_t *parcel)
{
  if (parcel)
    parcel_put(parcel);
}

/**
 * Allocate a new parcel and copy the passed parcel into it.
 */
parcel_t *
hpx_parcel_clone(parcel_t *parcel)
{
  size_t size = parcel_get_data_size(parcel);
  parcel_t *p = parcel_get(size);
  if (!p)
    return NULL;
  return hpx_parcel_copy(p, parcel);
}

/**
 * Copy @p from to @p to. Copies the maximum amount of data possible when the @p
 * to data is a different size than the @p from data. Zeros out the @p to data
 * block if there is extra space.
 */
parcel_t *
hpx_parcel_copy(parcel_t * restrict to,
                const parcel_t * restrict from)
{
  dbg_assert_precondition(to);
  dbg_assert_precondition(from);
  
  size_t to_size = parcel_get_data_size(to);
  size_t from_size = parcel_get_data_size(from);
  size_t min_size = (to_size < from_size) ? to_size : from_size;

  to->action = from->action;
  to->target = from->target;
  to->cont   = from->cont;
  memcpy(to->data, from->data, min_size);
  bzero((uint8_t*)to->data + min_size, to_size - min_size);
  return to;
}                     

/**
 * Sends the parcel.
 *
 * If the parcel is targeted at a NULL address, or a local address, then we
 * create a new local thread, otherwise we ask the network to send the parcel on
 * our behalf.
 */
int
hpx_parcel_send(locality_t *dest, const parcel_t *parcel,
                future_t **complete,
                future_t **thread,
                future_t **result)
{
  dbg_assert_precondition(dest);
  dbg_assert_precondition(parcel);
  
  if (complete)
    *complete = hpx_future_create(0);    
  if (thread)
    *thread = hpx_future_create(sizeof(thread_t *));
  if (result)
    *result = NULL;

  /* if the address is local, then just invoke the local action, on this path we
     can set the complete and thread futures synchronously, before returning */
  void* target = NULL;
  /* TODO: this won't work because we don't have a virtual-to-physical mapping
     */
  if (hpx_addr_get_local(parcel->target, &target)) {
    struct hpx_thread *t = NULL;
    hpx_func_t f = action_lookup(parcel->action);
    if (!f) {
      dbg_print_error(HPX_ERROR, "Could not find an action registered for %"
                      HPX_PRIu_hpx_action_t "\n", parcel->action);
    }
    
    int e = hpx_thread_create(NULL, HPX_THREAD_OPT_NONE, f, parcel->data,
                              result, &t);
    if (e)
      return e;
    if (complete)
      hpx_future_set(*complete);
    if (thread)
      hpx_future_setv(*thread, sizeof(t), &t);
    return HPX_SUCCESS;
  }

  /* otherwise the address is remote, we'll have to make a request from the
     network service */  
  return parcelhandler_send(dest, parcel, complete, thread, result);
}

/**
 * Resizes the data segment for the parcel. The current implementation doesn't
 * shrink the data block if @p size is less than the parcel's existing data
 * size.
 *
 * @param[in][out] parcel - the parcel to resize
 * @param[in]        size - the new size
 *
 * @returns HPX_SUCCESS or an error code
 */
int
hpx_parcel_resize(parcel_t **parcel, size_t size)
{
  dbg_assert_precondition(parcel);

  size_t psize = parcel_get_data_size(*parcel);
  if (psize >= size)
    return HPX_SUCCESS;

  parcel_t *p = hpx_parcel_acquire(size);
  if (!p)
    return __hpx_errno;

  hpx_parcel_copy(p, *parcel);
  hpx_parcel_release(*parcel);
  *parcel = p;
  return HPX_SUCCESS;
}

addr_t
hpx_parcel_get_target(const parcel_t *parcel)
{
  return parcel->target;
}

void
hpx_parcel_set_target(parcel_t *parcel, addr_t address)
{
  parcel->target = address;
}

addr_t
hpx_parcel_get_cont(const parcel_t *parcel)
{
  return parcel->cont;
}

void
hpx_parcel_set_cont(parcel_t *parcel, addr_t address)
{
  parcel->cont = address;
}

hpx_action_t
hpx_parcel_get_action(const parcel_t *parcel)
{
  return parcel->action;
}

void
hpx_parcel_set_action(parcel_t *parcel, const hpx_action_t action)
{
  parcel->action = action;
}

void *
hpx_parcel_get_data(parcel_t *parcel)
{
  return parcel->data;
}
