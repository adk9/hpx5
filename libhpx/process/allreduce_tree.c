//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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
#include <string.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include <libhpx/gas.h>
#include <libhpx/network.h>
#include "allreduce_tree.h"

// allreduce tree algorithm
// a)-- init phase --
// 1. init basic structures for root and all other nodes
//    (initially all leaves point to root node)
//
// b)-- subscribe --
// 1. leafs extend local continuations and local reductions
// 2. each leaf (only first one for each locality) notify root
// 3. root registers locals
//
// c)--subscribe finalize --
// 1. build tree on root
// 2. send parent link to each node (for reduction except root)
// 3. send continuation to each node (for broadcast except leaves)
// 4. setup parent node and extend continuation in parent

// do -> a.1
void allreduce_tree_init(allreduce_t *r, size_t bytes, hpx_addr_t parent,
                    hpx_monoid_id_t id, hpx_monoid_op_t op) {
  r->lock = hpx_lco_sema_new(1);
  r->bytes = bytes;
  r->parent = parent;
  if(!r->parent){
    // setup group information in parent only
    int group_bytes = sizeof(hpx_addr_t) * HPX_LOCALITIES;
    int ctx_bytes = sizeof(coll_t) + group_bytes;
    r->ctx = malloc(ctx_bytes);
    r->ctx->group_sz = 0 ;
    memset(r->ctx, 0, ctx_bytes);
  } else{
    r->ctx == NULL;	  
  }
  r->continuation = continuation_new(bytes);
  r->reduce = reduce_new(bytes, id, op);
  r->id = -1;
}

void allreduce_tree_fini(allreduce_t *r) {
  hpx_lco_delete_sync(r->lock);
  continuation_delete(r->continuation);
  reduce_delete(r->reduce);
  if(r->ctx){
    free(r->ctx);
  }
}

// do -> b.3
void allreduce_tree_register_leaves(allreduce_t *r, hpx_addr_t leaf_addr){
  dbg_assert(!r->parent);	
  dbg_assert(r->ctx);	
  
  // if i am the root then register leaf node gas locations
  int i = r->ctx->group_sz++;
  hpx_addr_t *locals = (hpx_addr_t*)r->ctx->data;
  locals[i] = leaf_addr;
}

// do -> b.1 , b.2 
int32_t allreduce_tree_add(allreduce_t *r, hpx_action_t op, hpx_addr_t addr) {
  int32_t i = 0;
  // acquire the semaphore
  hpx_lco_sema_p(r->lock);

  // extend the local continuation structure for (intra-node) leaves only and get and id for this input
  if(r->parent){
    i = continuation_add(&r->continuation, op, addr);
  }
  
  // extend the local reduction, if this is the first input tell root this locality 
  // exist (in order to build a tree)
  if (reduce_add(r->reduce) && r->parent) {
    hpx_addr_t allreduce = hpx_thread_current_target();
    dbg_check( hpx_call_sync(r->parent, allreduce_tree_register_leaves_async, NULL,
                             0, &allreduce) );
  }

  // release the lock
  hpx_lco_sema_v_sync(r->lock);
  return i;
}

// do --> c.4, c.5
int32_t allreduce_tree_setup_parent(allreduce_t *r, hpx_action_t op, hpx_addr_t child) {
  int32_t i = 0;
  // acquire the semaphore
  hpx_lco_sema_p(r->lock);

  // extend the local continuation structure for leaves only and get and id for this input
  i = continuation_add(&r->continuation, op, child);
  reduce_add(r->reduce);
  
  log_coll("[setup parent bcast] in rank :[%d] ga :[%llu] to child:[%llu] \n" , hpx_get_my_rank(), 
		  hpx_thread_current_target(), child);
  // release the lock
  hpx_lco_sema_v_sync(r->lock);
  return i;
}

// do --> c.4, c.5
void allreduce_tree_setup_child(allreduce_t *r, hpx_addr_t parent) {
  // acquire the semaphore
  hpx_lco_sema_p(r->lock);

  r->parent = parent;
  
  // extend the local reduction in parent 
  if (r->parent) {
    hpx_addr_t allreduce = hpx_thread_current_target();
    log_coll("[setup child link] in rank :[%d] ga :[%llu] to parent :[%llu] \n" , hpx_get_my_rank(), allreduce, parent);
    
    dbg_check( hpx_call_sync(r->parent, allreduce_tree_setup_parent_async, &r->id,
                             sizeof(int32_t), &allreduce_bcast_async, &allreduce) );
  }

  // release the lock
  hpx_lco_sema_v_sync(r->lock);
}

static int _has_parent(int locality){
	//every locality except 0 has a parent
	return locality != 0 ;	
}

// do --> c.1, c.2, c.3
// executed on root
void allreduce_tree_algo_nary(allreduce_t *r, hpx_addr_t *locals,
	       	int32_t num_locals, int32_t arity) {
  int i;
  int locality;
  
  log_coll("starting allreduce-nary tree algorithm locals : %d  arity : %d \n", num_locals, arity);
  if(num_locals < arity){
    log_coll("allreduce nary-tree algorithm doesn't support arity value: %d for %d num of localities..", 
		    arity, num_locals);
    log_coll("resetting arity value to : %d \n", num_locals);
    arity = num_locals;
  }
  /*we count root network node too*/
  int num_locals_with_root = num_locals + 1;
  hpx_addr_t root_ga = hpx_thread_current_target();

  hpx_addr_t group_locals[num_locals_with_root];

  // asign network root for first in group
  group_locals[0] = root_ga;
  //assign others in order
  for (i = 1; i <= num_locals; ++i) {
    group_locals[i] = locals[i - 1]; 	
  }

  hpx_addr_t null = HPX_NULL;
  hpx_addr_t and = hpx_lco_and_new(num_locals_with_root);

  // map locality index to tree nodes
  for (locality = 0; locality < num_locals_with_root; ++locality) {
    //reduce
    if(_has_parent(locality)){
      int parent = (locality % arity) == 0 ? (locality/arity) - 1 : (locality/arity);	
      hpx_addr_t parent_ga = group_locals[parent];

      //set parent in locality and extend bcast continuation in parent
      hpx_call(group_locals[locality], allreduce_tree_setup_child_async, and,  &parent_ga) ;
      log_coll("locality vrank :[%d] ga :[%llu] send --> to : vrank :[%d] ga : [%llu] \n" , locality, 
		      group_locals[locality], parent, parent_ga);
    } else {
      //set parent in root locality 
      hpx_call(root_ga, allreduce_tree_setup_child_async, and,  &null) ;
      log_coll("locality vrank :[%d] ga :[%llu] is parent\n" , locality, root_ga);
    }
  }
  hpx_lco_wait_reset(and);
  hpx_lco_delete_sync(and);
  
  /*printf("nary algorithm setup completed\n");*/
}
