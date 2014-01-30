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
#include "hpx/globals.h"                        /* __hpx_global_ctx */
#include "hpx/parcel.h"                         /* hpx_parcel_* */
#include "hpx/thread.h"                         /* hpx_thread_create */
#include "action.h"                             /* action_lookup */
#include "address.h"                            /* get_local_address */
#include "allocator.h"                          /* parcel_put/get */
#include "debug.h"                              /* dbg_ suff */
#include "init.h"                               /* hpx_parcel_init/fini */
#include "parcel.h"                             /* struct hpx_parcel */
#include "parcelhandler.h"                      /* parcelhandler_send */

struct hpx_locality;
struct hpx_thread;

/** Parcel subsystem initializer and finalizer.
 *  @{
 */
hpx_error_t
hpx_parcel_init(void)
{
  return HPX_SUCCESS;
}

void
hpx_parcel_fini(void)
{
  /* shutdown the parcel handler thread */
}
/** @} */

/**
 * Forward the request for a parcel to the parcel allocator.
 */
hpx_parcel_t *
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
hpx_parcel_release(hpx_parcel_t *parcel)
{
  if (parcel)
    parcel_put(parcel);
}

/**
 * Allocate a new parcel and copy the passed parcel into it.
 */
hpx_parcel_t *
hpx_parcel_clone(hpx_parcel_t *parcel)
{
  size_t size = parcel_get_data_size(parcel);
  hpx_parcel_t *p = parcel_get(size);
  if (!p)
    return NULL;
  return hpx_parcel_copy(p, parcel);
}

/**
 * Copy @p from to @p to. Copies the maximum amount of data possible when the @p
 * to data is a different size than the @p from data. Zeros out the @p to data
 * block if there is extra space.
 */
hpx_parcel_t *
hpx_parcel_copy(hpx_parcel_t * restrict to,
                const hpx_parcel_t * restrict from)
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
 * Implement hpx_parcel_send as a local thread spawn.
 */
static int
local_send(const hpx_parcel_t *parcel,
           hpx_future_t *complete,
           hpx_future_t *thread,
           hpx_future_t **result)
{
  /* allocate any extra futures that the sender wants */
  hpx_func_t f = action_lookup(parcel->action);
  if (!f)
    dbg_print_error(HPX_ERROR, "Could not find an action registered for %"
                    HPX_PRIu_hpx_action_t "\n", parcel->action);

  /* LD: hpx_thread_create does not know what to do with the argument data, so
     for now we copy and leak it.
     TODO: the entire thread creation pipeline needs to be fixed
  */
  uint8_t *data = NULL;
  if (parcel->size) {
    data = hpx_alloc(parcel->size);
    memcpy(data, parcel->data, parcel->size);
    /* dbg_printf("FIXME: Leaking %lu bytes of data in 'local_send'\n", */
    /*            parcel->size);  */
  }

  struct hpx_thread *t = NULL;
  int e = hpx_thread_create(__hpx_global_ctx, HPX_THREAD_OPT_NONE, f, data,
                            result, &t);  

  /* if necessary, signal that the send is complete both locally and globally */
  if (complete)
    hpx_future_set(complete);
  if (thread)
    hpx_future_setv(thread, sizeof(t), &t);
  return e;
}

/**
 * Send the parcel.
 *
 * If the parcel is targeted at a NULL address, or a local address, then we
 * create a new local thread, otherwise we ask the network to send the parcel on
 * our behalf.
 */
int
hpx_parcel_send(struct hpx_locality *dest, const hpx_parcel_t *parcel,
                hpx_future_t *complete,
                hpx_future_t *thread,
                hpx_future_t **result)
{
  dbg_assert_precondition(dest);
  dbg_assert_precondition(parcel);

  return (dest->rank == hpx_get_rank()) ?
    /* (hpx_locality_equal(hpx_get_my_locality(), dest)) ? */
    local_send(parcel, complete, thread, result) : 
    parcelhandler_send(dest, parcel, complete, thread, result);
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
hpx_parcel_resize(hpx_parcel_t **parcel, size_t size)
{
  dbg_assert_precondition(parcel);

  size_t psize = parcel_get_data_size(*parcel);
  if (psize >= size)
    return HPX_SUCCESS;

  hpx_parcel_t *p = hpx_parcel_acquire(size);
  if (!p)
    return __hpx_errno;

  hpx_parcel_copy(p, *parcel);
  hpx_parcel_release(*parcel);
  *parcel = p;
  return HPX_SUCCESS;
}

hpx_addr_t
hpx_parcel_get_target(const hpx_parcel_t *parcel)
{
  return parcel->target;
}

void
hpx_parcel_set_target(hpx_parcel_t *parcel, hpx_addr_t address)
{
  parcel->target = address;
}

hpx_addr_t
hpx_parcel_get_cont(const hpx_parcel_t *parcel)
{
  return parcel->cont;
}

void
hpx_parcel_set_cont(hpx_parcel_t *parcel, hpx_addr_t address)
{
  parcel->cont = address;
}

hpx_action_t
hpx_parcel_get_action(const hpx_parcel_t *parcel)
{
  return parcel->action;
}

void
hpx_parcel_set_action(hpx_parcel_t *parcel, const hpx_action_t action)
{
  parcel->action = action;
}

void *
hpx_parcel_get_data(hpx_parcel_t *parcel)
{
  return parcel->data;
}

void
hpx_parcel_set_data(hpx_parcel_t * restrict parcel, void * restrict data, size_t length)
{
  dbg_assert_precondition(parcel);
  dbg_assert_precondition(length <= parcel->size);
  dbg_assert_precondition(!data || length);
  
  memcpy(parcel->data, data, length);
}

/**
 * From libhpx/parcel.h.
 */
size_t
parcel_get_data_size(const hpx_parcel_t *parcel)
{
  return parcel->size;
}
