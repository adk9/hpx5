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

#ifndef LIBHPX_COALESCED_NETWORK_H
#define LIBHPX_COALESCED_NETWORK_H

#include <stdlib.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libsync/queues.h>
#include <string.h>

#include "libsync/locks.h"
#include "libhpx/parcel.h"
#include "libhpx/network.h"
#include "libhpx/memory.h"
#include "libsync/sync.h"

#include <libhpx/gas.h>
#include <inttypes.h>
#include <libhpx/libhpx.h>

/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
struct transport;
/// @}

#define COALESCING_SIZE 10000 //TODO: coalescing size should be a command line parameter
typedef unsigned char byte_t;


//hpx_parcel_t* _coalesced_network_progress(void *obj);
static int coalesced_network_send(void *network,  hpx_parcel_t *p);
static int coalesced_network_progress(void *obj, int id);
static uint64_t coalesced_network_parcel_queue_size(void *network);
static uint64_t coalesced_network_buffer_size(void *obj);
static int coalesced_network_pwc(void *obj, hpx_addr_t to, const void *from, size_t n,
		   hpx_action_t lop, hpx_addr_t laddr, hpx_action_t rop,
				 hpx_addr_t raddr);
static int coalesced_network_put(void *obj, hpx_addr_t to, const void *from, size_t n,
				 hpx_action_t lop, hpx_addr_t laddr);
static int coalesced_network_get(void *obj, void *to, hpx_addr_t from, size_t n,
				 hpx_action_t lop, hpx_addr_t laddr);
static hpx_parcel_t* coalesced_network_probe(void *obj, int rank);
static void coalesced_network_set_flush(void *obj);
static void coalesced_network_register_dma(void *obj, const void *base, size_t bytes, void *key);
static void coalesced_network_release_dma(void *obj, const void *base, size_t bytes);
static int coalesced_network_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset);
static int coalesced_network_lco_wait(void *obj, hpx_addr_t lco, int reset);
static void coalesced_network_delete(void *obj);
static int coalesced_network_command(void *obj, hpx_addr_t locality, hpx_action_t op,
				     uint64_t args);

static int coalesced_network_pwc(void *obj, hpx_addr_t to, const void *from, size_t n,
		   hpx_action_t lop, hpx_addr_t laddr, hpx_action_t rop,
				 hpx_addr_t raddr);
static int coalesced_network_put(void *obj, hpx_addr_t to, const void *from, size_t n,
				 hpx_action_t lop, hpx_addr_t laddr);
static int coalesced_network_get(void *obj, void *to, hpx_addr_t from, size_t n,
				 hpx_action_t lop, hpx_addr_t laddr);
static hpx_parcel_t* coalesced_network_probe(void *obj, int rank);
static void coalesced_network_set_flush(void *obj);
static void coalesced_network_register_dma(void *obj, const void *base, size_t bytes, void *key);
static void coalesced_network_release_dma(void *obj, const void *base, size_t bytes);
static int coalesced_network_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset);
static int coalesced_network_lco_wait(void *obj, hpx_addr_t lco, int reset);
static void coalesced_network_delete(void *obj);
static int coalesced_network_command(void *obj, hpx_addr_t locality, hpx_action_t op,
				     uint64_t args);

typedef struct coalesced_network{
  network_t       vtable;
  network_t *base_network;
  two_lock_queue_t sends;
  uint64_t previous_queue_size;
  uint64_t count_parcels;
 } coalesced_network_t;


static network_t* coalesced_network_new (network_t *network) { 
  coalesced_network_t *coalesced_network = (coalesced_network_t *) malloc(sizeof(coalesced_network_t));
  
  //set the base network
  coalesced_network->base_network = network;

  // initialize the local coalescing queue for the parcels
  sync_two_lock_queue_init(&coalesced_network->sends, NULL);
  sync_store(&coalesced_network->sends.size, 0, SYNC_RELAXED);

  coalesced_network->previous_queue_size = 0;
  coalesced_network->count_parcels = 0;

  coalesced_network->vtable.delete = coalesced_network_delete;
  coalesced_network->vtable.progress = coalesced_network_progress;
  coalesced_network->vtable.send =  coalesced_network_send;
  coalesced_network->vtable.command = coalesced_network_command; 
  coalesced_network->vtable.pwc = coalesced_network_pwc;
  coalesced_network->vtable.put = coalesced_network_put; 
  coalesced_network->vtable.get = coalesced_network_get;
  coalesced_network->vtable.probe = coalesced_network_probe; 
  coalesced_network->vtable.set_flush =  coalesced_network_set_flush;
  coalesced_network->vtable.register_dma = coalesced_network_register_dma;
  coalesced_network->vtable.release_dma = coalesced_network_release_dma; 
  coalesced_network->vtable.lco_get = coalesced_network_lco_get;
  coalesced_network->vtable.lco_wait = coalesced_network_lco_wait;

  //coalesced_network->vtable.network_parcel_queue_size = coalesced_network_parcel_queue_size;
  //coalesced_network->vtable.network_buffer_size = coalesced_network_buffer_size;

  printf("Created coalescing network\n");
  return ((network_t*) coalesced_network);
}

static void coalesced_network_delete(void *obj) {
  coalesced_network_t *coalesced_network = obj;
  dbg_assert(coalesced_network);
  network_delete(coalesced_network->base_network);
}

//demultiplexing action on the receiver side
static int _demultiplex_message_handler(void* fatparcel, size_t n) {
  //retrieve the next parcel from the fat parcel
  hpx_parcel_t* p = fatparcel;
  //printf("In demultiplexer action\n");
  uint32_t current_size = n;
  uint32_t small_parcel_size  = 0;
  hpx_parcel_t *clone = NULL;
  hpx_parcel_t *next = NULL;
  next = p;
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


static int coalesced_network_command(void *obj, hpx_addr_t locality, hpx_action_t op,
		   uint64_t args) {
  coalesced_network_t *coalesced_network = obj;
  return network_command(coalesced_network->base_network, locality, op, args);
}

static uint64_t coalesced_network_parcel_queue_size(void *network) {
  coalesced_network_t *coalesced_network = network; 
  return sync_load(&coalesced_network->sends.size, SYNC_RELAXED);
}

static void _send_all(coalesced_network_t *coalesced_network) {
  //assert(destination_buffer_size);
  hpx_parcel_t *p = NULL;
  uint64_t number_of_parcels_dequeued = 0;
  uint64_t i = 0;

  //TODO; optimization later: if I am the locality, then skip my entries
  if(coalesced_network_parcel_queue_size(coalesced_network) > 0) {
    //printf("queue size of coalesced send %"PRIu64 " \n", coalesced_network_parcel_queue_size(coalesced_network));

   //allocate an array to get bytecount per destination
   uint32_t* total_byte_count = (uint32_t *) malloc (HPX_LOCALITIES * sizeof(uint32_t));
   for(i = 0; i < HPX_LOCALITIES; i++ ) {
     total_byte_count[i] = 0;
   }

   //allocate array for maintaining how many parcels per destination in the coalesced buffers and initialize them
   uint32_t* destination_buffer_size = (uint32_t *) malloc (HPX_LOCALITIES * sizeof(uint32_t));
   for (i = 0; i < HPX_LOCALITIES; i++) {
     destination_buffer_size[i] = 0;
   }


   //adjust the queue size before we proceed to copying out parcels from the queue to local buffer
   uint64_t current_coalescing_queue_size = coalesced_network_parcel_queue_size(coalesced_network);
   uint64_t readjusted_coalescing_queue_size = coalesced_network_parcel_queue_size(coalesced_network) - current_coalescing_queue_size;
   while (true) {
     uint64_t current_size = coalesced_network_parcel_queue_size(coalesced_network);
     if(current_size >= current_coalescing_queue_size) {
       uint64_t viewed_coalescing_queue_size = sync_cas_val(&coalesced_network->sends.size, current_size, readjusted_coalescing_queue_size, SYNC_RELAXED, SYNC_RELAXED);
       if (viewed_coalescing_queue_size == current_size) {
	 break;
       } else {
	 continue;
       }       
     } else {
       break;
     }
   }

   //temporarily copy content of the queue into a buffer and get an estimation of how many buffers need to be allocated per destination.
   hpx_parcel_t *coalesced_chain = NULL;
   while (current_coalescing_queue_size > 0) {
     p = sync_two_lock_queue_dequeue(&coalesced_network->sends);
     if(p==NULL)
       break;
     //printf("temporarily copying contents\n");

     //append to the chain
     parcel_stack_push(&coalesced_chain, p);
     number_of_parcels_dequeued++ ;
     //check the parcel destination
     uint64_t destination = gas_owner_of(here->gas, p->target);
     //increase the  destination buffer size
     destination_buffer_size[destination] += 1; 
     total_byte_count[destination] += parcel_size(p);
     current_coalescing_queue_size--;
   }

   //allocate buffers for each destination
   char** coalesced_buffer = (char **) malloc (HPX_LOCALITIES * sizeof(char *));

   for (i = 0; i < HPX_LOCALITIES; i++) {
     coalesced_buffer[i] = (char *) malloc (total_byte_count[i] * sizeof(char));
   }

   //allocate array for current buffer index position for each destination
   uint32_t* current_destination_buffer_index;
   current_destination_buffer_index = (uint32_t *) malloc (HPX_LOCALITIES * sizeof(uint32_t));
  
   for (i = 0; i < HPX_LOCALITIES; i++) {
     current_destination_buffer_index[i] = 0;
   }

   //printf("Sorting the parcels according to destinations\n");
   uint32_t n = 0;
  
   //Now, sort the parcels to destination bin
   for (i = 0; i < number_of_parcels_dequeued; i++) {
     //printf("Processing parcel and putting it into the right destination buffer\n");
     p = NULL;
     p = parcel_stack_pop(&coalesced_chain);
     uint64_t destination = gas_owner_of(here->gas, p->target);
     n = parcel_size(p);
     memcpy(coalesced_buffer[destination] + current_destination_buffer_index[destination], p, n);
     current_destination_buffer_index[destination] += n;
   }

  
   //printf("number_of_parcels_dequeued %" PRIu64 "\n", number_of_parcels_dequeued);
  
   //again zeroing out the current position buffer
   for (i = 0; i < HPX_LOCALITIES; i++) {
     current_destination_buffer_index[i] = 0;
   }
 
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
 }
}

static int coalesced_network_send(void *network,  hpx_parcel_t *p) {
  coalesced_network_t *coalesced_network = network; 
  //printf("In coalesced network send\n");
  uint64_t coalescing_size =  COALESCING_SIZE;
  //uint64_t prev_count = sync_load(&coalesced_network->count_parcels, SYNC_RELAXED);
  uint64_t prev_count = sync_cas_val(&coalesced_network->count_parcels, coalescing_size, 0, SYNC_RELAXED, SYNC_RELAXED);
 
  //Before putting the parcel in the queue, check whether the queue size is the same as the coalescing size 
  if(prev_count == COALESCING_SIZE) {
    //then we empty the queue
    uint64_t old_coalescing_queue_size = sync_load(&coalesced_network->previous_queue_size, SYNC_RELAXED);
    uint64_t seen_previous_queue_size = sync_cas_val(&coalesced_network->previous_queue_size, old_coalescing_queue_size, 0, SYNC_RELAXED, SYNC_RELAXED);
    _send_all(coalesced_network);
  } else {
    sync_fadd(&coalesced_network->count_parcels, 1, SYNC_RELAXED);
  }

  //Otherwise put the parcel in the coalesced send queue
  sync_two_lock_queue_enqueue(&coalesced_network->sends, p);
  sync_fadd(&coalesced_network->sends.size, 1, SYNC_RELAXED);
  if(prev_count != COALESCING_SIZE) {
    //check whether we can update the previous parcel count for flush condition 
    uint64_t old_coalescing_queue_size = sync_load(&coalesced_network->previous_queue_size, SYNC_RELAXED);
    uint64_t seen_previous_queue_size = sync_cas_val(&coalesced_network->previous_queue_size, old_coalescing_queue_size, coalesced_network->sends.size, SYNC_RELAXED, SYNC_RELAXED); 
  }
 
  //printf("Returning from coalescing network send\n");
  return LIBHPX_OK;
}

static int coalesced_network_progress(void *obj, int id) {
  coalesced_network_t *coalesced_network = obj;
  assert(coalesced_network);
  //printf("In coalescing network progress\n");

  //check whether the queue has not grown since last time visited
  uint64_t current_coalescing_queue_size = coalesced_network_parcel_queue_size(coalesced_network);
  uint64_t previous_coalescing_queue_size =  sync_cas_val(&coalesced_network->previous_queue_size, current_coalescing_queue_size, 0,  SYNC_RELAXED, SYNC_RELAXED);
  
  if((previous_coalescing_queue_size == current_coalescing_queue_size && coalesced_network_parcel_queue_size(coalesced_network) > 0)) {
    //if that is the case, then flush outstanding buffer
    sync_store(&coalesced_network->count_parcels, 0,  SYNC_RELAXED);
    _send_all(coalesced_network);
  }

  //Then call underlying base network progress function
  return network_progress(coalesced_network->base_network, 0);
}

static int coalesced_network_pwc(void *obj, hpx_addr_t to, const void *from, size_t n,
		   hpx_action_t lop, hpx_addr_t laddr, hpx_action_t rop,
		   hpx_addr_t raddr) {
  coalesced_network_t *coalesced_network = obj;
  return network_pwc(coalesced_network->base_network, to, from, n, lop, laddr, rop, raddr);
}

static int coalesced_network_put(void *obj, hpx_addr_t to, const void *from, size_t n,
		   hpx_action_t lop, hpx_addr_t laddr){
  coalesced_network_t *coalesced_network = obj;
  return network_put(coalesced_network->base_network, to, from, n, lop, laddr);
}

static int coalesced_network_get(void *obj, void *to, hpx_addr_t from, size_t n,
		   hpx_action_t lop, hpx_addr_t laddr) {
  coalesced_network_t *coalesced_network = obj;
  return network_get(coalesced_network->base_network, to, from, n, lop, laddr);
}

static hpx_parcel_t* coalesced_network_probe(void *obj, int rank) {
  coalesced_network_t *coalesced_network = obj;
  return network_probe(coalesced_network->base_network, rank);
}

static void coalesced_network_set_flush(void *obj) {
  coalesced_network_t *coalesced_network = obj;
  network_flush_on_shutdown(coalesced_network->base_network);
}

static void coalesced_network_register_dma(void *obj, const void *base, size_t bytes, void *key) {
  coalesced_network_t *coalesced_network = obj;
  network_register_dma(coalesced_network->base_network, base, bytes, key);
}

static void coalesced_network_release_dma(void *obj, const void *base, size_t bytes) {
  coalesced_network_t *coalesced_network = obj;
  network_release_dma(coalesced_network->base_network, base, bytes);
}

static int coalesced_network_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset) {
  coalesced_network_t *coalesced_network = obj;
  return network_lco_get(coalesced_network->base_network, lco, n, out, reset);
}

static int coalesced_network_lco_wait(void *obj, hpx_addr_t lco, int reset) {
  coalesced_network_t *coalesced_network = obj;
  return network_lco_wait(coalesced_network->base_network, lco, reset);
}


/* static inline uint64_t network_send_buffer_size(void *obj) { */
/* coalesced_network_t *coalesced_network = obj; */
/*   return coalesced_network->base_network.send_buffer_size(coalesced_network->base_network); */
/* } */

static uint64_t coalesced_network_buffer_size(void *obj) {
  coalesced_network_t *coalesced_network = obj;
  return network_send_buffer_size(coalesced_network->base_network);
}


/* static inline uint64_t network_parcel_queue_size(void *obj) { */
/* coalesced_network_t *coalesced_network = obj; */
/*   return coalesced_network->base_network.parcel_queue_size(coalesced_network->base_network); */
/* } */


#endif // LIBHPX_COALESCED_NETWORK_H

