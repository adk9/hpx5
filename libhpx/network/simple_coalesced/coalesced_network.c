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

#include <stdlib.h>
#include <inttypes.h>
#include <libhpx/libhpx.h>
#include <libsync/queues.h>
#include <string.h>

#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/config.h>

#include "libsync/locks.h"
#include "libhpx/parcel.h"
#include "libhpx/network.h"
#include "libhpx/memory.h"
#include "libsync/sync.h"

#include <libhpx/padding.h>
#include <libhpx/gas.h>


#define COALESCING_SIZE 10000 //TODO: coalescing size should be a command line parameter

typedef struct coalesced_network{
  network_t       vtable;
  network_t *base_network;
  two_lock_queue_t sends;
  uint64_t parcel_count;
  uint64_t previous_parcel_count;
  uint64_t coalescing_size;
 } _coalesced_network_t;



static void _coalesced_network_delete(void *obj) {
  _coalesced_network_t *coalesced_network = obj;
  dbg_assert(coalesced_network);
  network_delete(coalesced_network->base_network);
}

//demultiplexing action on the receiver side
static int _demultiplex_message_handler(void* fatparcel, size_t n) {
  //retrieve the next parcel from the fat parcel
  hpx_parcel_t* next = fatparcel;
  //printf("In demultiplexer action\n");
  uint32_t current_size = n;
  uint32_t small_parcel_size  = 0;
  hpx_parcel_t *clone = NULL;
  while (current_size > 0) {
    //assuming that after src we have the size field, get the size of the next parcel
    small_parcel_size = parcel_size(next); //get the next parcel size
    clone = parcel_clone(next);
    parcel_launch(clone);
    next = (hpx_parcel_t*) (((char*) next) + (small_parcel_size ));
    current_size -= small_parcel_size;
    clone = NULL;
  }
  return HPX_SUCCESS;
}

static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _demultiplexer, _demultiplex_message_handler, HPX_POINTER, HPX_SIZE_T);

static void _send_n(_coalesced_network_t *coalesced_network, uint64_t current_parcel_count) {
  //assert(destination_buffer_size);
  hpx_parcel_t *p = NULL;
  uint64_t number_of_parcels_dequeued = 0;
  uint64_t i = 0;
  uint64_t parcel_to_be_dequeued_count = current_parcel_count;

  //printf("queue size of coalesced send %"PRIu64 " \n", coalesced_network_parcel_queue_size(coalesced_network));

  //allocate an array to get bytecount per destination
  uint32_t* total_byte_count = (uint32_t *) calloc (HPX_LOCALITIES, sizeof(uint32_t));

  //allocate array for maintaining how many parcels per destination in the coalesced buffers and initialize them
  uint32_t* destination_buffer_size = (uint32_t *) calloc (HPX_LOCALITIES, sizeof(uint32_t));

  //temporarily copy content of the queue into a buffer and get an estimation of how many buffers need to be allocated per destination.
  hpx_parcel_t *coalesced_chain = NULL;
  while (parcel_to_be_dequeued_count > 0) {
    p = sync_two_lock_queue_dequeue(&coalesced_network->sends);
    //if(p==NULL)
    //  break;
    //printf("temporarily copying contents\n");

    //append to the chain
    parcel_stack_push(&coalesced_chain, p);
    number_of_parcels_dequeued++ ;
    //check the parcel destination
    uint64_t destination = gas_owner_of(here->gas, p->target);
    //increase the  destination buffer size
    destination_buffer_size[destination] += 1;
    total_byte_count[destination] += parcel_size(p);
    parcel_to_be_dequeued_count--;
  }

  //allocate buffers for each destination
  char** coalesced_buffer = (char **) malloc (HPX_LOCALITIES * sizeof(char *));

  for (uint64_t i = 0; i < HPX_LOCALITIES; i++) {
    coalesced_buffer[i] = (char *) malloc (total_byte_count[i] * sizeof(char));
  }

  //allocate array for current buffer index position for each destination
  uint32_t* current_destination_buffer_index = (uint32_t *) calloc (HPX_LOCALITIES, sizeof(uint32_t));

  //printf("Sorting the parcels according to destinations\n");
  uint32_t n = 0;

  //Now, sort the parcels to destination bin
  for (uint64_t i = 0; i < number_of_parcels_dequeued; i++) {
    //printf("Processing parcel and putting it into the right destination buffer\n");
    p = NULL;
    p = parcel_stack_pop(&coalesced_chain);
    uint64_t destination = gas_owner_of(here->gas, p->target);
    n = parcel_size(p);
    memcpy(coalesced_buffer[destination] + current_destination_buffer_index[destination], p, n);
    current_destination_buffer_index[destination] += n;
  }


  //printf("number_of_parcels_dequeued %" PRIu64 "\n", number_of_parcels_dequeued);


  //printf("Creating fat parcels\n");
  //create fat parcel for each destination and call base network interface to send it
  uint64_t rank = 0;
  for (rank = 0; rank < HPX_LOCALITIES; rank++) {
    //check whether the particular destination has any parcel to recieve ie. check if zero parcel count
    if(destination_buffer_size[rank] == 0) {
      continue;
    }

    //create the fat parcel
    hpx_pid_t pid = hpx_thread_current_pid();
    hpx_addr_t target = HPX_THERE(rank);
    hpx_parcel_t *p = parcel_new(target, _demultiplexer, 0, 0, pid, coalesced_buffer[rank] , total_byte_count[rank]);
    _prepare(p);
    //printf("Created a fat parcel\n");
    //call base network send interface
    //printf("Calling base network send for sending fat parcel\n");
    network_send(coalesced_network->base_network, p);
  }

  //clean up code
  //printf("cleaning up after one network send\n");
  for (i = 0; i < HPX_LOCALITIES; i++) {
    free(coalesced_buffer[i]);
  }
  free(coalesced_buffer);

  free(total_byte_count);
  free(destination_buffer_size);
  free(current_destination_buffer_index);
}

static int _coalesced_network_send(void *network,  hpx_parcel_t *p) {
  _coalesced_network_t *coalesced_network = network;
  //printf("In coalesced network send\n");

  //Before putting the parcel in the queue, check whether the queue size has reached the  coalescing size then we empty the queue. If that is the case, then try to adjust the parcel count before we proceed to creating fat parcels

  uint64_t parcel_count = sync_load(&coalesced_network->parcel_count,  SYNC_RELAXED);
  while ( parcel_count >= coalesced_network->coalescing_size ) {
    uint64_t readjusted_parcel_count = parcel_count - coalesced_network->coalescing_size;
    uint64_t viewed_parcel_count = sync_cas_val(&coalesced_network->parcel_count, parcel_count, readjusted_parcel_count, SYNC_RELAXED, SYNC_RELAXED);
    if (viewed_parcel_count == parcel_count) {
      //flush outstanding buffer
      _send_n(coalesced_network, coalesced_network->coalescing_size);
      break;
    }
    parcel_count = sync_load(&coalesced_network->parcel_count,  SYNC_RELAXED);
  }

  //Put the parcel in the coalesced send queue
  sync_fadd(&coalesced_network->parcel_count, 1, SYNC_RELAXED);
  sync_two_lock_queue_enqueue(&coalesced_network->sends, p);
  sync_store(&coalesced_network->previous_parcel_count, coalesced_network->parcel_count, SYNC_RELAXED);

  //printf("Returning from coalescing network send\n");
  return LIBHPX_OK;
}

static int _coalesced_network_progress(void *obj, int id) {
  _coalesced_network_t *coalesced_network = obj;
  assert(coalesced_network);
  //printf("In coalescing network progress\n");

  //check whether the queue has not grown since the last time
  uint64_t current_parcel_count = sync_load(&coalesced_network->parcel_count, SYNC_RELAXED);
  uint64_t previous_parcel_count =  sync_cas_val(&coalesced_network->previous_parcel_count, current_parcel_count, 0,  SYNC_RELAXED, SYNC_RELAXED);

  //if that is the case, then try to adjust the parcel count before we proceed to creating fat parcels
  while (previous_parcel_count == current_parcel_count && current_parcel_count > 0) {
    uint64_t viewed_parcel_count = sync_cas_val(&coalesced_network->parcel_count, current_parcel_count, 0, SYNC_RELAXED, SYNC_RELAXED);
    if (viewed_parcel_count == current_parcel_count) {
      //flush outstanding buffer
      _send_n(coalesced_network, current_parcel_count);
      break;
    }
    current_parcel_count = sync_load(&coalesced_network->parcel_count, SYNC_RELAXED);
  }

  //Then call the underlying base network progress function
  return network_progress(coalesced_network->base_network, 0);
}

static int _coalesced_network_command(void *obj, hpx_addr_t locality, hpx_action_t op,
		   uint64_t args) {
  _coalesced_network_t *coalesced_network = obj;
  return network_command(coalesced_network->base_network, locality, op, args);
}

static int _coalesced_network_pwc(void *obj, hpx_addr_t to, const void *from, size_t n,
		   hpx_action_t lop, hpx_addr_t laddr, hpx_action_t rop,
		   hpx_addr_t raddr) {
  _coalesced_network_t *coalesced_network = obj;
  return network_pwc(coalesced_network->base_network, to, from, n, lop, laddr, rop, raddr);
}

static int _coalesced_network_put(void *obj, hpx_addr_t to, const void *from, size_t n,
		   hpx_action_t lop, hpx_addr_t laddr){
  _coalesced_network_t *coalesced_network = obj;
  return network_put(coalesced_network->base_network, to, from, n, lop, laddr);
}

static int _coalesced_network_get(void *obj, void *to, hpx_addr_t from, size_t n,
		   hpx_action_t lop, hpx_addr_t laddr) {
  _coalesced_network_t *coalesced_network = obj;
  return network_get(coalesced_network->base_network, to, from, n, lop, laddr);
}

static hpx_parcel_t* _coalesced_network_probe(void *obj, int rank) {
  _coalesced_network_t *coalesced_network = obj;
  return network_probe(coalesced_network->base_network, rank);
}

static void _coalesced_network_set_flush(void *obj) {
  _coalesced_network_t *coalesced_network = obj;
  //network_flush_on_shutdown(coalesced_network->base_network);
}

static void _coalesced_network_register_dma(void *obj, const void *base, size_t bytes, void *key) {
  _coalesced_network_t *coalesced_network = obj;
  network_register_dma(coalesced_network->base_network, base, bytes, key);
}

static void _coalesced_network_release_dma(void *obj, const void *base, size_t bytes) {
  _coalesced_network_t *coalesced_network = obj;
  network_release_dma(coalesced_network->base_network, base, bytes);
}

static int _coalesced_network_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset) {
  _coalesced_network_t *coalesced_network = obj;
  return network_lco_get(coalesced_network->base_network, lco, n, out, reset);
}

static int _coalesced_network_lco_wait(void *obj, hpx_addr_t lco, int reset) {
  _coalesced_network_t *coalesced_network = obj;
  return network_lco_wait(coalesced_network->base_network, lco, reset);
}

static uint64_t _coalesced_network_buffer_size(void *obj) {
  _coalesced_network_t *coalesced_network = obj;
  //return network_send_buffer_size(coalesced_network->base_network);
}

network_t* coalesced_network_new (network_t *network,  const struct config *cfg) {
  _coalesced_network_t *coalesced_network = NULL;
  posix_memalign((void*)&coalesced_network, HPX_CACHELINE_SIZE, sizeof(*coalesced_network));
  if (!coalesced_network) {
    log_error("could not allocate a coalesced network\n");
    return NULL;
  }

  //set the base network
  coalesced_network->base_network = network;

  // initialize the local coalescing queue for the parcels
  sync_two_lock_queue_init(&coalesced_network->sends, NULL);

  //set coalescing size
  coalesced_network->coalescing_size = cfg->coalescing_buffersize;

  coalesced_network->parcel_count = 0;
  coalesced_network->previous_parcel_count = 0;

  //set the vtable
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

  //coalesced_network->vtable.network_parcel_queue_size = coalesced_network_parcel_queue_size;
  //coalesced_network->vtable.network_buffer_size = coalesced_network_buffer_size;

  printf("Created coalescing network\n");
  //return ((network_t*) coalesced_network);
  return &coalesced_network->vtable;
}
