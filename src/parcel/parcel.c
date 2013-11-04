#include <stdint.h>                             /* uint */
#include <string.h>                             /* memcpy, bzero */
#include <stdio.h>                              /* fprintf */

#include "libhpx/debug.h"                       /* DEBUG */
#include "libhpx/parcel.h"                      /* struct hpx_parcel */
#include "allocator.h"                          /* parcel_{get,set} */
#include "hpx2/action.h"                        /* hpx_action_invoke */
#include "hpx2/error.h"                         /* HPX_{SUCCESS,ERROR} */
#include "hpx2/future.h"                        /* hpx_future_* */
#include "hpx2/parcel.h"                        /* hpx_parcel_* */
#include "hpx2/thread.h"                        /* hpx_thread_create */

/**
 * Forward the request for a parcel to the parcel allocator.
 */
struct hpx_parcel *
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
hpx_parcel_release(struct hpx_parcel *parcel)
{
  if (parcel)
    parcel_put(parcel);
}

/**
 * Allocate a new parcel and copy the passed parcel into it.
 */
struct hpx_parcel *
hpx_parcel_clone(struct hpx_parcel *parcel)
{
  size_t size = parcel_get_data_size(parcel);
  struct hpx_parcel *p = parcel_get(size);
  if (!p)
    return NULL;
  return hpx_parcel_copy(p, parcel);
}

/**
 * Copy @p from to @p to. Copies the maximum amount of data possible when the @p
 * to data is a different size than the @p from data. Zeros out the @p to data
 * block if there is extra data.
 */
struct hpx_parcel *
hpx_parcel_copy(struct hpx_parcel * restrict to,
                const struct hpx_parcel * restrict from)
{
  size_t to_size = parcel_get_data_size(to);
  size_t from_size = parcel_get_data_size(from);
  size_t min_size = (to_size < from_size) ? to_size : from_size;

  to->action = from->action;
  to->address = from->address;
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
hpx_parcel_send(const struct hpx_parcel * const parcel,
                struct hpx_future ** const complete,
                struct hpx_future ** const thread,
                struct hpx_future ** const result)
{
  /* initialize the out parameters */
  if (complete)
    *complete = hpx_future_create();    
  if (thread)
    *thread = hpx_future_create();
  if (result)
    *result = NULL;

  /* if the address is local, then just invoke the local action, on this path we
     can set the complete and thread futures synchronously, before returning */
  void* target = NULL;
  if (get_local_address(&parcel->address, &target)) {
    struct hpx_thread *t = NULL;
    hpx_func_t f = hpx_action_lookup(parcel->action);
    if (!f) {
      if (DEBUG)
        fprintf(stderr,
                "Could not find an action registered for %p\n", parcel->action);
      return HPX_ERROR;
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
  return hpx_network_send(parcel, complete, thread, result);
}

/**
 * Resizes the data segment for the parcel. The current implementation doesn't
 * shrink the data block if @p size is less than the parcel's existing data
 * size.
 */
int
hpx_parcel_resize(struct hpx_parcel **parcel, size_t size)
{
  size_t psize = parcel_get_data_size(*parcel);
  if (psize >= size)
    return 0;

  struct hpx_parcel *p = hpx_parcel_acquire(size);
  if (!p)
    return -1;

  hpx_parcel_copy(p, *parcel);
  hpx_parcel_release(*parcel);
  *parcel = p;
  return 0;
}

struct hpx_address * const
hpx_parcel_address(struct hpx_parcel * const parcel)
{
  return &parcel->address;
}

hpx_action_t
hpx_parcel_get_action(const struct hpx_parcel * const parcel)
{
  return parcel->action;
}

void
hpx_parcel_set_action(struct hpx_parcel * const parcel,
                      const hpx_action_t action)
{
  parcel->action = action;
}

void * const
hpx_parcel_get_data(struct hpx_parcel * const parcel)
{
  return parcel->data;
}
