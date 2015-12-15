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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <hpx/hpx.h>
#include <libsync/queues.h>
#include <libsync/sync.h>

#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>

typedef struct {
  network_t               vtable;
  network_t        *base_network;
  two_lock_queue_t         sends;
  uint64_t          parcel_count;
  uint64_t previous_parcel_count;
  uint64_t       coalescing_size;
} _coalesced_network_t;

static void _coalesced_network_delete(void *obj) {
  _coalesced_network_t *coalesced_network = obj;
  dbg_assert(coalesced_network);
  network_delete(coalesced_network->base_network);
  free(obj);
}

//  demultiplexing action on the receiver side
static int _demultiplex_message_handler(void* fatparcel, size_t remaining) {
  //  retrieve the next parcel from the fat parcel
  hpx_parcel_t* next = fatparcel;
  while (remaining > 0) {
    uint32_t next_parcel_size = parcel_size(next);
    hpx_parcel_t *clone = parcel_clone(next);
    parcel_launch(clone);
    next = (hpx_parcel_t*) (((char*) next) + (next_parcel_size));
    remaining -= next_parcel_size;
    dbg_assert(remaining >= 0);
  }
  return HPX_SUCCESS;
}

static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _demultiplexer,
		     _demultiplex_message_handler, HPX_POINTER, HPX_SIZE_T);

static void _send_n(_coalesced_network_t *coalesced_network, uint64_t n) {
  uint64_t parcel_to_be_dequeued_count = n;

  //  allocate an array to get bytecount per destination
  uint32_t* total_byte_count = calloc (HPX_LOCALITIES, sizeof(uint32_t));

  //  allocate array for maintaining how many parcels per destination in the
  //  coalesced buffers and initialize them
  uint32_t* destination_buffer_size = calloc (HPX_LOCALITIES, sizeof(uint32_t));

  //  temporarily copy content of the queue into a buffer and get an estimation
  //  of how many buffers need to be allocated per destination.
  hpx_parcel_t *coalesced_chain = NULL;
  for (; parcel_to_be_dequeued_count > 0; parcel_to_be_dequeued_count--) {
    hpx_parcel_t *p = sync_two_lock_queue_dequeue(&coalesced_network->sends);
    dbg_assert(p);
    parcel_stack_push(&coalesced_chain, p);
    uint64_t destination = gas_owner_of(here->gas, p->target);
    destination_buffer_size[destination] += 1;
    total_byte_count[destination] += parcel_size(p);
  }

  //  allocate buffers for each destination
  char** coalesced_buffer = (char **) malloc (HPX_LOCALITIES * sizeof(char *));

  for (uint64_t i = 0; i < HPX_LOCALITIES; i++) {
    coalesced_buffer[i] = (char *) malloc (total_byte_count[i] * sizeof(char));
  }

  //  allocate array for current buffer index position for each destination
  uint32_t* current_destination_buffer_index = calloc (HPX_LOCALITIES,
						       sizeof(uint32_t));

   //  Now, sort the parcels to destination bin
  for (uint64_t i = 0; i < n; i++) {
    hpx_parcel_t *p = parcel_stack_pop(&coalesced_chain);
    uint64_t destination = gas_owner_of(here->gas, p->target);
    size_t current_parcel_size = parcel_size(p);
    memcpy(coalesced_buffer[destination] +
	   current_destination_buffer_index[destination], p,
	   current_parcel_size);
    current_destination_buffer_index[destination] += current_parcel_size;
  }

  //  create fat parcel for each destination and call base network interface to
  //  send it
  uint64_t rank = 0;
  for (rank = 0; rank < HPX_LOCALITIES; rank++) {
    //  check whether the particular destination has any parcel to recieve ie.
    //  check if zero parcel count
    if(destination_buffer_size[rank] == 0) {
      continue;
    }

    //  create the fat parcel
    hpx_pid_t pid = hpx_thread_current_pid();
    hpx_addr_t target = HPX_THERE(rank);
    hpx_parcel_t *p = parcel_new(target, _demultiplexer, 0, 0, pid,
				 coalesced_buffer[rank] ,
				 total_byte_count[rank]);
    parcel_prepare(p);
    //  call base network send interface
    network_send(coalesced_network->base_network, p);
  }

  //  clean up code
  for (uint64_t i = 0; i < HPX_LOCALITIES; i++) {
    free(coalesced_buffer[i]);
  }
  free(coalesced_buffer);

  free(total_byte_count);
  free(destination_buffer_size);
  free(current_destination_buffer_index);
}

static int _coalesced_network_send(void *network,  hpx_parcel_t *p) {
  _coalesced_network_t *coalesced_network = network;
  if (!action_is_coalesced(p->action)) {
    network_send(coalesced_network->base_network, p);
    return LIBHPX_OK;
  }

  //  Before putting the parcel in the queue, check whether the queue size has
  //  reached the  coalescing size then we empty the queue. If that is the case,
  //  then try to adjust the parcel count before we proceed to creating fat
  //  parcels

  uint64_t parcel_count = sync_load(&coalesced_network->parcel_count,
				    SYNC_RELAXED);
  while ( parcel_count >= coalesced_network->coalescing_size ) {
    uint64_t readjusted_parcel_count =
      parcel_count - coalesced_network->coalescing_size;
    uint64_t temp_parcel_count = parcel_count;
    uint64_t viewed_parcel_count =
      sync_cas_val(&coalesced_network->parcel_count,
                   temp_parcel_count,
                   readjusted_parcel_count,
                   SYNC_RELAXED, SYNC_RELAXED);
    if (viewed_parcel_count == parcel_count) {
      //  flush outstanding buffer
      _send_n(coalesced_network, coalesced_network->coalescing_size);
      break;
    }
    parcel_count = sync_load(&coalesced_network->parcel_count,  SYNC_RELAXED);
  }

  //  Put the parcel in the coalesced send queue
  dbg_assert(p);
  sync_two_lock_queue_enqueue(&coalesced_network->sends, p);
  sync_fadd(&coalesced_network->parcel_count, 1, SYNC_RELAXED);
  return LIBHPX_OK;
}

static int _coalesced_network_progress(void *obj, int id) {
  _coalesced_network_t *coalesced_network = obj;
  assert(coalesced_network);

  //  check whether the queue has not grown since the last time
  uint64_t current_parcel_count = sync_load(&coalesced_network->parcel_count,
					    SYNC_RELAXED);
  uint64_t previous_parcel_count =
    sync_load(&coalesced_network->previous_parcel_count, SYNC_RELAXED);

  //  if that is the case, then try to adjust the parcel count before we proceed
  //  to creating fat parcels
  while (previous_parcel_count == current_parcel_count &&
	 current_parcel_count > 0) {
    uint64_t temp_parcel_count = current_parcel_count;
    uint64_t viewed_parcel_count =
      sync_cas_val(&coalesced_network->parcel_count,
                   temp_parcel_count, 0,
                   SYNC_RELAXED, SYNC_RELAXED);
    if (viewed_parcel_count == current_parcel_count) {
      //  flush outstanding buffer
      _send_n(coalesced_network, current_parcel_count);
      break;
    }
    current_parcel_count = sync_load(&coalesced_network->parcel_count,
				     SYNC_RELAXED);
  }

  sync_store(&coalesced_network->previous_parcel_count,
	     coalesced_network->parcel_count, SYNC_RELAXED);

  //  Then call the underlying base network progress function
  return network_progress(coalesced_network->base_network, 0);
}

static int _coalesced_network_command(void *obj, hpx_addr_t locality,
				      hpx_action_t op, uint64_t args) {
  _coalesced_network_t *coalesced_network = obj;
  return network_command(coalesced_network->base_network, locality, op, args);
}

static int _coalesced_network_pwc(void *obj, hpx_addr_t to, const void *from,
				  size_t n, hpx_action_t lop, hpx_addr_t laddr,
				  hpx_action_t rop, hpx_addr_t raddr) {
  _coalesced_network_t *coalesced_network = obj;
  return network_pwc(coalesced_network->base_network, to, from, n, lop, laddr,
		     rop, raddr);
}

static int _coalesced_network_put(void *obj, hpx_addr_t to, const void *from,
				  size_t n, hpx_action_t lop, hpx_addr_t laddr){
  _coalesced_network_t *coalesced_network = obj;
  return network_put(coalesced_network->base_network, to, from, n, lop, laddr);
}

static int _coalesced_network_get(void *obj, void *to, hpx_addr_t from,
				  size_t n, hpx_action_t lop, hpx_addr_t laddr)
{
  _coalesced_network_t *coalesced_network = obj;
  return network_get(coalesced_network->base_network, to, from, n, lop, laddr);
}

static hpx_parcel_t* _coalesced_network_probe(void *obj, int rank) {
  _coalesced_network_t *coalesced_network = obj;
  return network_probe(coalesced_network->base_network, rank);
}

static void _coalesced_network_set_flush(void *obj) {
  _coalesced_network_t *coalesced_network = obj;
  uint64_t count = sync_swap(&coalesced_network->parcel_count, 0, SYNC_RELAXED);
  _send_n(coalesced_network, count);

  coalesced_network->base_network->flush(coalesced_network->base_network);
}

static void _coalesced_network_register_dma(void *obj, const void *base,
					    size_t bytes, void *key) {
  _coalesced_network_t *coalesced_network = obj;
  network_register_dma(coalesced_network->base_network, base, bytes, key);
}

static void _coalesced_network_release_dma(void *obj, const void *base,
					   size_t bytes) {
  _coalesced_network_t *coalesced_network = obj;
  network_release_dma(coalesced_network->base_network, base, bytes);
}

static int _coalesced_network_lco_get(void *obj, hpx_addr_t lco, size_t n,
				      void *out, int reset) {
  _coalesced_network_t *coalesced_network = obj;
  return network_lco_get(coalesced_network->base_network, lco, n, out, reset);
}

static int _coalesced_network_lco_wait(void *obj, hpx_addr_t lco, int reset) {
  _coalesced_network_t *coalesced_network = obj;
  return network_lco_wait(coalesced_network->base_network, lco, reset);
}

network_t* coalesced_network_new (network_t *network,  const struct config *cfg)
{
  _coalesced_network_t *coalesced_network = NULL;
  posix_memalign((void*)&coalesced_network, HPX_CACHELINE_SIZE,
		 sizeof(*coalesced_network));
  if (!coalesced_network) {
    log_error("could not allocate a coalesced network\n");
    return NULL;
  }

  //  set the vtable
  coalesced_network->vtable.delete = _coalesced_network_delete;
  coalesced_network->vtable.progress = _coalesced_network_progress;
  coalesced_network->vtable.send =  _coalesced_network_send;
  coalesced_network->vtable.command = _coalesced_network_command;
  coalesced_network->vtable.pwc = _coalesced_network_pwc;
  coalesced_network->vtable.put = _coalesced_network_put;
  coalesced_network->vtable.get = _coalesced_network_get;
  coalesced_network->vtable.probe = _coalesced_network_probe;
  coalesced_network->vtable.flush =  _coalesced_network_set_flush;
  coalesced_network->vtable.register_dma = _coalesced_network_register_dma;
  coalesced_network->vtable.release_dma = _coalesced_network_release_dma;
  coalesced_network->vtable.lco_get = _coalesced_network_lco_get;
  coalesced_network->vtable.lco_wait = _coalesced_network_lco_wait;

  //  set the base network
  coalesced_network->base_network = network;

  //  initialize the local coalescing queue for the parcels
  sync_two_lock_queue_init(&coalesced_network->sends, NULL);

  //  set coalescing size
  coalesced_network->coalescing_size = cfg->coalescing_buffersize;

  coalesced_network->parcel_count = 0;
  coalesced_network->previous_parcel_count = 0;

  log_net("Created coalescing network\n");
  return &coalesced_network->vtable;
}
