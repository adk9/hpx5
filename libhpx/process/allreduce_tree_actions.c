// =============================================================================
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

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include "allreduce_tree.h"



static int _allreduce_tree_init_handler(allreduce_t *r, size_t bytes,
                                   hpx_addr_t parent, hpx_action_t id,
                                   hpx_action_t op) {
  CHECK_ACTION(id);
  CHECK_ACTION(op);
  hpx_monoid_id_t rid = (hpx_monoid_id_t)actions[id].handler;
  hpx_monoid_op_t rop = (hpx_monoid_op_t)actions[op].handler;
  allreduce_tree_init(r, bytes, parent, rid, rop);
  return HPX_SUCCESS;
}

HPX_ACTION(HPX_INTERRUPT, HPX_PINNED, allreduce_tree_init_async,
           _allreduce_tree_init_handler, HPX_POINTER, HPX_SIZE_T, HPX_ADDR,
           HPX_ACTION_T, HPX_ACTION_T);



static int _allreduce_tree_add_handler(allreduce_t *r, hpx_action_t op,
                                  hpx_addr_t addr) {
  int32_t i = allreduce_tree_add(r, op, addr);
  return HPX_THREAD_CONTINUE(i);
}
HPX_ACTION(HPX_DEFAULT, HPX_PINNED, allreduce_tree_add_async,
           _allreduce_tree_add_handler, HPX_POINTER, HPX_ACTION_T, HPX_ADDR);


static int _allreduce_tree_register_leaves_handler(allreduce_t *r, hpx_addr_t addr) {
  allreduce_tree_register_leaves(r, addr);
  return HPX_SUCCESS;
}
HPX_ACTION(HPX_DEFAULT, HPX_PINNED, allreduce_tree_register_leaves_async,
           _allreduce_tree_register_leaves_handler, HPX_POINTER, HPX_ADDR);

static int _allreduce_tree_fini_handler(allreduce_t *r) {
  allreduce_tree_fini(r);
  return HPX_SUCCESS;
}

HPX_ACTION(HPX_DEFAULT, HPX_PINNED, allreduce_tree_fini_async,
           _allreduce_tree_fini_handler, HPX_POINTER);



static int _allreduce_tree_algo_nary_handler(allreduce_t *r, int arity){ 
  //assert this is the root	
  dbg_assert(!r->parent);
    
  int32_t num_locals    = r->ctx->group_sz;
  hpx_addr_t* locals = (hpx_addr_t*) (r->ctx->data); 
  allreduce_tree_algo_nary(r, locals, num_locals, arity);
  return HPX_SUCCESS;
}

HPX_ACTION(HPX_DEFAULT, HPX_PINNED, allreduce_tree_algo_nary_async,
           _allreduce_tree_algo_nary_handler,  HPX_POINTER, HPX_INT);

static int _allreduce_tree_algo_binomial_handler(allreduce_t *r){ 
  //assert this is the root	
  dbg_assert(!r->parent);
    
  int32_t num_locals    = r->ctx->group_sz;
  hpx_addr_t* locals = (hpx_addr_t*) (r->ctx->data); 
  allreduce_tree_algo_binomial(r, locals, num_locals);
  return HPX_SUCCESS;
}

HPX_ACTION(HPX_DEFAULT, HPX_PINNED, allreduce_tree_algo_binomial_async,
           _allreduce_tree_algo_binomial_handler,  HPX_POINTER);

static int _allreduce_tree_setup_parent_handler(allreduce_t *r, hpx_action_t op, hpx_addr_t child) {
  int32_t i ;	
  i = allreduce_tree_setup_parent(r, op, child);
  return HPX_THREAD_CONTINUE(i);
}

HPX_ACTION(HPX_DEFAULT, HPX_PINNED, allreduce_tree_setup_parent_async,
           _allreduce_tree_setup_parent_handler, HPX_POINTER, HPX_ACTION_T, HPX_ADDR);


static int _allreduce_tree_setup_child_handler(allreduce_t *r, hpx_addr_t parent) {
  allreduce_tree_setup_child(r, parent);	
  return HPX_SUCCESS;
}

HPX_ACTION(HPX_DEFAULT, HPX_PINNED, allreduce_tree_setup_child_async,
	   _allreduce_tree_setup_child_handler, HPX_POINTER, HPX_ADDR);


