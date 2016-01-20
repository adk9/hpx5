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
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include <libhpx/gas.h>
#include "allreduce.h"

void allreduce_init(allreduce_t *r, size_t bytes, hpx_addr_t parent,
                    hpx_monoid_id_t id, hpx_monoid_op_t op) {
  r->lock = hpx_lco_sema_new(1);
  r->bytes = bytes;
  r->parent = parent;
  r->continuation = continuation_new(bytes);
  r->reduce = reduce_new(bytes, id, op);
  r->id = -1;
  //allocate memory for data structure plus for rank data
  //optimistic allocation for ranks - for all lcoalities
  int ctx_bytes= sizeof(coll_t) + sizeof(int32_t) * HPX_LOCALITIES;
  r->ctx = malloc(ctx_bytes);
  r->ctx->group_bytes = sizeof(int32_t) * HPX_LOCALITIES;
  r->ctx->comm_bytes = 0 ;
  r->ctx->group_sz = 0 ;
  r->ctx->recv_count = bytes;
  r->ctx->type = ALL_REDUCE;
  r->ctx->op = op;
}

void allreduce_fini(allreduce_t *r) {
  hpx_lco_delete_sync(r->lock);
  continuation_delete(r->continuation);
  reduce_delete(r->reduce);
}

#include "inttypes.h"

int32_t allreduce_add(allreduce_t *r, hpx_action_t op, hpx_addr_t addr) {
  int32_t i = 0;
  // acquire the semaphore
  hpx_lco_sema_p(r->lock);

  // extend the local continuation structure and get and id for this input
  i = continuation_add(&r->continuation, op, addr);

  //if i am the root then add leaf node into to active locations
  if(!r->parent){
   int i = r->ctx->group_sz++;
   /*r->ctx.group[i] = addr;*/
   int32_t* ranks = (int32_t*)r->ctx->data;
   if(here->ranks > 1){
     ranks[i] =  gas_owner_of(here->gas, addr);
   }else {
     //smp mode
     ranks[i] = 0 ;
   }	
   /*printf("root add location : %"PRId64"  idx : %d \n", r->loc[i], i);*/
  }	
  // extend the local reduction, if this is the first input then we need to
  // recursively tell our parent (if we have one) that we exist, and that we
  // need to have our bcast action run as a continuation
  if (reduce_add(r->reduce) && r->parent) {
    hpx_addr_t allreduce = hpx_thread_current_target();
    /*printf("====non root location : %"PRId64"  parent : %"PRId64" \n", allreduce, r->parent);*/
    dbg_check( hpx_call_sync(r->parent, allreduce_add_async, &r->id,
                             sizeof(r->id), &allreduce_bcast_async,
                             &allreduce) );
  }

  // release the lock
  hpx_lco_sema_v_sync(r->lock);
  return i;
}

void allreduce_remove(allreduce_t *r, int32_t id) {
  // acquire the lock
  hpx_lco_sema_p(r->lock);

  // remove the continuation that is leaving
  continuation_remove(&r->continuation, id);

  // remove this input from our allreduce, if this is the last input then tell
  // our parent that we are no longer participating
  if (reduce_remove(r->reduce) && r->parent) {
    dbg_check( hpx_call_sync(r->parent, allreduce_remove_async, NULL, 0,
                             &r->id) );
    r->id = -1;
  }

  // release the lock
  hpx_lco_sema_v_sync(r->lock);
}

#include <libhpx/network.h>
#define SW_COLLECTIVES 1

void allreduce_reduce(allreduce_t *r, const void *val) {
  log_coll("reducing at %p\n", r);
  // if this isn't the last local value then just continue
  if (!reduce_join(r->reduce, val)) {
    return;
  }

#ifdef SW_COLLECTIVES
  //for sw based direct collective join
  //create parcel and prepare for coll call
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, r->bytes);
  void *output = malloc(r->bytes);

  if (r->parent) {
    reduce_reset(r->reduce, hpx_parcel_get_data(p));
  }
  else {
    reduce_reset(r->reduce, output);
  }

  /*int* input = (int*)hpx_parcel_get_data(p);*/
  //perform synchronized collective comm
  here->network->coll_sync(here->network, p, output, r->ctx);

  /*int* res = (int*) output;*/
  /**res = 240;*/
  //call all local continuations to communicate the result
  continuation_trigger(r->continuation, output);
  /*printf("reduce done ...current_node : %"PRId64" out bytes : %d output : %d input : %d \n", hpx_thread_current_target(), */
		  /*cl.recv_count, *((int*)output), *input);*/

  free(output);
  return;
#endif
  // the local continuation is done, join the parent node asynchronously
  if (r->parent) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, r->bytes);
    p->target = r->parent;
    p->action = allreduce_join_async;
    reduce_reset(r->reduce, hpx_parcel_get_data(p));
    parcel_launch(p);
    return;
  }

  // this is a root node, so turn around and run all of our continuations
  void *result = malloc(r->bytes);
  reduce_reset(r->reduce, result);
  allreduce_bcast(r, result);
  free(result);
}

void allreduce_bcast(allreduce_t *r, const void *value) {
  log_coll("broadcasting at %p\n", r);
  // just trigger the continuation stored in this node
  continuation_trigger(r->continuation, value);
}

void allreduce_bcast_comm(allreduce_t *r, hpx_addr_t base, const void *coll) {
  log_coll("broadcasting comm ranks from root %p\n", r);
  
  //boradcast my comm group to all leaves
  //this is executed only on network root
  if(coll == NULL){
    int n = here->ranks;
    /*printf("[root] broadcast task current_node : %"PRId64"  n_loc : %d nranks : %d \n", hpx_thread_current_target(), r->ctx->group_sz, n );*/
    hpx_addr_t target = HPX_NULL; 
    hpx_addr_t and = hpx_lco_and_new(n);
    for (int i = 0; i < n; ++i) {
      if(here->rank != i){	    
	target = hpx_addr_add(base, i * sizeof(allreduce_t), sizeof(allreduce_t));  
	hpx_call(target, allreduce_bcast_comm_async, and, r->ctx, (sizeof(coll_t) + r->ctx->group_bytes)); 	
      }
    }
    
    target = hpx_addr_add(base, here->rank * sizeof(allreduce_t), sizeof(allreduce_t));  
    //order sends to remote rank first and local rank then; here
    //provided network is flushed ,this will facilitate blocking call 
    //for a collective group creation in bcast_comm
    hpx_call(target, allreduce_bcast_comm_async, and, r->ctx, sizeof(coll_t) + r->ctx->group_bytes); 	
    
    hpx_lco_wait(and);
    hpx_lco_delete_sync(and);
     
#if 0
    int32_t* ranks = (int32_t*)r->ctx->data;
    for (int i = 0; i < r->ctx->group_sz; ++i) {
       printf("broadcast task current_node : %"PRId64"  idx: %d  target rank : %d \n", 
		       hpx_thread_current_target(), i, ranks[i] );
    }
#endif
    return;
  }
  
  //set the collective context in current leaf node
  //this is executed only in leaves 
  const coll_t* c = coll;
  *r->ctx = *c;
  dbg_assert(r->ctx->group_sz = c->group_sz);
  dbg_assert(r->ctx->recv_count = c->recv_count);

  int32_t* ranks = (int32_t*)r->ctx->data;
  int32_t* copy_ranks = (int32_t*) c->data;
  for (int i = 0; i < c->group_sz; ++i) {
    ranks[i] = copy_ranks[i];    
  }
  //perform collective initialization for all leaf nodes here
  dbg_check(here->network->coll_init(here->network, &r->ctx));
  /*printf("[leaf] broadcast task current_node : %"PRId64"  n_loc : %d myrank : %d \n", hpx_thread_current_target(), r->ctx->group_sz, here->rank );*/
  
}
