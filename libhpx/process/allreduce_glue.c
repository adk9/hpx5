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

#include <libhpx/debug.h>
#include <libhpx/events.h>
#include <libhpx/instrumentation.h>
#include <libhpx/locality.h>
#include "allreduce.h"
#include "allreduce_tree.h"

/*allreduce_algo_t allred_mode = TREE_FLAT ;*/
allreduce_algo_t allred_mode = TREE_NARY;

// arity for allreduce
// TODO : change to a config option later
int arity_allred_nary_mode = 2;

static const size_t BSIZE = sizeof(allreduce_t);

hpx_addr_t hpx_process_collective_allreduce_new(size_t bytes,
                                                hpx_action_t reset,
                                                hpx_action_t op) {
  // allocate and initialize a root node
  hpx_addr_t root = hpx_gas_alloc_local(1, BSIZE, 0);
  dbg_assert(root);
  hpx_addr_t null = HPX_NULL;

  if(allred_mode == TREE_FLAT){
    dbg_check( hpx_call_sync(root, allreduce_init_async, NULL, 0, &bytes, &null,
                           &reset, &op) );
  } else if(allred_mode == TREE_NARY){
    dbg_check( hpx_call_sync(root, allreduce_tree_init_async, NULL, 0, &bytes, &null,
                           &reset, &op) );
  } else{
    dbg_error("mode : %d NOT supported for allreduce!\n", allred_mode);
  }
  // allocate an array of local elements for the process
  int n = here->ranks;
  hpx_addr_t base = hpx_gas_alloc_cyclic(n, BSIZE, 0);
  dbg_assert(base);

  hpx_addr_t and = hpx_lco_and_new(n);
  if(allred_mode == TREE_FLAT){
    // initialize the array to point to the root as their parent (fat tree)
    dbg_check( hpx_gas_bcast_with_continuation(allreduce_init_async, base, n,
                                             0, BSIZE, hpx_lco_set_action, and,
                                             &bytes, &root, &reset, &op) );
  } else if(allred_mode == TREE_NARY){
    dbg_check( hpx_gas_bcast_with_continuation(allreduce_tree_init_async, base, n,
                                             0, BSIZE, hpx_lco_set_action, and,
                                             &bytes, &root, &reset, &op) );

  } else{
    dbg_error("mode : %d NOT supported for allreduce!\n", allred_mode);
  }
    
  hpx_lco_wait(and);
  hpx_lco_delete_sync(and);

  // return the array
  EVENT_COLLECTIVE_NEW(base);
  return base;
}

void hpx_process_collective_allreduce_delete(hpx_addr_t allreduce) {
  hpx_addr_t root = HPX_NULL;
  hpx_addr_t proxy = hpx_addr_add(allreduce, here->rank * BSIZE, BSIZE);
  allreduce_t *r = NULL;
  if (!hpx_gas_try_pin(proxy, (void*)&r)) {
    dbg_error("could not pin local element for an allreduce\n");
  }
  root = r->parent;
  hpx_gas_unpin(proxy);

  int n = here->ranks;
  hpx_addr_t and = hpx_lco_and_new(n + 1);
  if(allred_mode == TREE_FLAT){
    dbg_check( hpx_gas_bcast_with_continuation(allreduce_fini_async, allreduce,
                                             n, 0, BSIZE, hpx_lco_set_action,
                                             and) );
    dbg_check( hpx_call(root, allreduce_fini_async, and) );
  } else if (allred_mode == TREE_NARY){
    dbg_check( hpx_gas_bcast_with_continuation(allreduce_tree_fini_async, allreduce,
                                             n, 0, BSIZE, hpx_lco_set_action,
                                             and) );
    dbg_check( hpx_call(root, allreduce_tree_fini_async, and) );
	  
  } else{
    dbg_error("mode : %d NOT supported for allreduce!\n", allred_mode);
  }

  hpx_lco_wait(and);
  hpx_lco_delete_sync(and);

  hpx_gas_free_sync(root);
  hpx_gas_free_sync(allreduce);
}

int32_t hpx_process_collective_allreduce_subscribe(hpx_addr_t allreduce,
                                                   hpx_action_t c_action,
                                                   hpx_addr_t c_target) {
  int id;
  hpx_addr_t leaf = hpx_addr_add(allreduce, here->rank * BSIZE, BSIZE);
  if(allred_mode == TREE_FLAT){
    dbg_check( hpx_call_sync(leaf, allreduce_add_async, &id, sizeof(id),
                           &c_action, &c_target) );
  } else if(allred_mode == TREE_NARY) {
    dbg_check( hpx_call_sync(leaf, allreduce_tree_add_async, &id, sizeof(id),
                           &c_action, &c_target) );
  } else {
    dbg_error("mode : %d NOT supported for allreduce!\n", allred_mode);
  }
    
  EVENT_COLLECTIVE_SUBSCRIBE(allreduce, c_action, c_target, id, here->rank);
  return id;
}

int hpx_process_collective_allreduce_subscribe_finalize(hpx_addr_t allreduce) {
  allreduce_t *r = NULL;
  hpx_addr_t root = HPX_NULL;
  hpx_addr_t leaf = hpx_addr_add(allreduce, here->rank * BSIZE, BSIZE);
  if (!hpx_gas_try_pin(leaf, (void *)&r)) {
    dbg_error("could not pin local element for an allreduce\n");
  }
  root = r->parent;

  if(allred_mode == TREE_FLAT){
    if (here->config->coll_network) {
      dbg_check(hpx_call_sync(root, allreduce_bcast_comm_async, NULL, 0, &allreduce,
                          sizeof(hpx_addr_t)));
    }
  } else if(allred_mode == TREE_NARY){
      dbg_check(hpx_call_sync(root, allreduce_tree_algo_nary_async, NULL, 0, &arity_allred_nary_mode));
  } else{
    dbg_error("mode : %d NOT supported for allreduce!\n", allred_mode);
  } 
  hpx_gas_unpin(leaf);
  return HPX_SUCCESS;
}

void hpx_process_collective_allreduce_unsubscribe(hpx_addr_t allreduce,
                                                  int32_t id) {
  hpx_addr_t leaf = hpx_addr_add(allreduce, here->rank * BSIZE, BSIZE);
  dbg_check( hpx_call_sync(leaf, allreduce_remove_async, NULL, 0, &id) );
  EVENT_COLLECTIVE_UNSUBSCRIBE(allreduce, id, here->rank);
}

int hpx_process_collective_allreduce_join(hpx_addr_t allreduce,
                                          int32_t id, size_t bytes,
                                          const void *in) {
  hpx_addr_t proxy = hpx_addr_add(allreduce, here->rank * BSIZE, BSIZE);
  allreduce_t *r = NULL;
  if (!hpx_gas_try_pin(proxy, (void*)&r)) {
    dbg_error("could not pin local element for an allreduce\n");
  }
  allreduce_reduce(r, in);
  hpx_gas_unpin(proxy);
  EVENT_COLLECTIVE_JOIN(allreduce, proxy, bytes, id, here->rank);
  return HPX_SUCCESS;
}
