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
#include "allreduce.h"

static int _allreduce_init_handler(allreduce_t *r, size_t bytes,
                                   hpx_addr_t parent, hpx_action_t id,
                                   hpx_action_t op, int net_optype, int net_datatype) {
  CHECK_ACTION(id);
  CHECK_ACTION(op);
  hpx_monoid_id_t rid = (hpx_monoid_id_t)actions[id].handler;
  hpx_monoid_op_t rop = (hpx_monoid_op_t)actions[op].handler;
  allreduce_init(r, bytes, parent, rid, rop, (hpx_coll_optype_t) net_optype,
		 (hpx_coll_dtype_t) net_datatype);
  return HPX_SUCCESS;
}
HPX_ACTION(HPX_INTERRUPT, HPX_PINNED, allreduce_init_async,
           _allreduce_init_handler, HPX_POINTER, HPX_SIZE_T, HPX_ADDR,
           HPX_ACTION_T, HPX_ACTION_T, HPX_INT, HPX_INT);

static int _allreduce_fini_handler(allreduce_t *r) {
  allreduce_fini(r);
  return HPX_SUCCESS;
}
HPX_ACTION(HPX_DEFAULT, HPX_PINNED, allreduce_fini_async,
           _allreduce_fini_handler, HPX_POINTER);

static int _allreduce_add_handler(allreduce_t *r, hpx_action_t op,
                                  hpx_addr_t addr) {
  int32_t i = allreduce_add(r, op, addr);
  return HPX_THREAD_CONTINUE(i);
}
HPX_ACTION(HPX_DEFAULT, HPX_PINNED, allreduce_add_async,
           _allreduce_add_handler, HPX_POINTER, HPX_ACTION_T, HPX_ADDR);

static int _allreduce_remove_handler(allreduce_t *r, int32_t id) {
  allreduce_remove(r, id);
  return HPX_SUCCESS;
}
HPX_ACTION(HPX_DEFAULT, HPX_PINNED, allreduce_remove_async,
           _allreduce_remove_handler, HPX_POINTER, HPX_SINT32);

static int _allreduce_join_handler(allreduce_t *r, void *value, size_t bytes) {
  dbg_assert(bytes == r->bytes);
  allreduce_reduce(r, value);
  return HPX_SUCCESS;
}
HPX_ACTION(HPX_INTERRUPT, HPX_PINNED | HPX_MARSHALLED, allreduce_join_async,
           _allreduce_join_handler, HPX_POINTER, HPX_POINTER, HPX_SIZE_T);


static int _allreduce_bcast_comm_handler(allreduce_t *r, void *value,
                                         size_t bytes) {
  if (!r->parent) {
    // if root netwrk node we pass only the base address for bcast
    dbg_assert(bytes == sizeof(hpx_addr_t));
    hpx_addr_t base = *((hpx_addr_t *)value);

    allreduce_bcast_comm(r, base, NULL);
    return HPX_SUCCESS;
  }

  coll_t *ctx = value;
  dbg_assert(bytes == (sizeof(coll_t) + ctx->group_bytes));
  allreduce_bcast_comm(r, HPX_NULL, ctx);
  return HPX_SUCCESS;
}
HPX_ACTION(HPX_INTERRUPT, HPX_PINNED | HPX_MARSHALLED,
           allreduce_bcast_comm_async, _allreduce_bcast_comm_handler,
           HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

static int _allreduce_bcast_handler(allreduce_t *r, const void *value,
                                    size_t bytes) {
  dbg_assert(bytes == r->bytes);
  allreduce_bcast(r, value);
  return HPX_SUCCESS;
}
HPX_ACTION(HPX_INTERRUPT, HPX_PINNED | HPX_MARSHALLED, allreduce_bcast_async,
           _allreduce_bcast_handler, HPX_POINTER, HPX_POINTER, HPX_SIZE_T);
