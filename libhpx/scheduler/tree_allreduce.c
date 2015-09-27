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

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <libsync/locks.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include "lco.h"

#define K 7

typedef struct {
  int id;
  char data[];
} _join_args_t;

/// A wrapper function to perform a binary reduction using two parcel buffers.
static void _op(hpx_action_t act, hpx_parcel_t *lhs, hpx_parcel_t *rhs) {
  dbg_assert(act);
  void *to = hpx_parcel_get_data(lhs);
  void *from = hpx_parcel_get_data(rhs);
  hpx_action_handler_t f = action_table_get_handler(here->actions, act);
  hpx_monoid_op_t op = (hpx_monoid_op_t)f;
  op(to, from, lhs->size);
}

/// A tree node.
///
/// Each tree node takes up one cacheline and stores up to 7 values to
/// reduce. Tree nodes are always allocated in arrays, so we can figure out
/// parents and children as long as we know the index of the tree node.
typedef struct {
  int             count;
  int          children;
  hpx_parcel_t *parcels[K];
} _tree_node_t;

_HPX_ASSERT(sizeof(_tree_node_t) == HPX_CACHELINE_SIZE, _tree_node_unexpected_size);

/// The tree structure provides the LCO functions, as well as storing some
/// global information about the tree including the reduction callbacks, the
/// size of the value being reduced, the phase, and the array of nodes.
typedef struct {
  lco_t       lco;
  hpx_action_t op;
  int           n;
  int           N;
  int           I;
  PAD_TO_CACHELINE(sizeof(lco_t) +
                   sizeof(hpx_action_t) * 2 +
                   sizeof(int));
  _tree_node_t nodes[];
} _tree_t;

/// Get the node's parent node.
///
/// This just does some index and pointer arithmetic to figure out the address
/// of the node's parent. This is possible because the tree is stored as an
/// array, the node knows its index in the array, and the parent() relation has
/// a closed form solution for a K-ary tree stored in an array.
static _tree_node_t *_get_parent(_tree_t *tree, _tree_node_t *node) {
  int i = node - tree->nodes;
  if (i == 0) {
    return NULL;
  }
  int j = (i - 1) / K;
  dbg_assert(0 <= j && j < tree->N);
  return &tree->nodes[j];
}

/// Get the ith child o f the node.
///
/// This does some index and pointer arithmetic to figure out the address of the
/// node's ith child. This is possible because the tree is stored as an array,
/// the node knows its index in the array, and the child(c) relation has a
/// closed form for a K-ary tree stored in an array.
static _tree_node_t *_get_child(_tree_t *tree, _tree_node_t *node, int c) {
  int i = node - tree->nodes;
  int j = K * i + 1 + c;
  dbg_assert(0 <= j && j < tree->N);
  return &tree->nodes[j];
}

/// Figure out how many inputs (i.e., children), a node has.
///
/// This is possible because we know the index of the node in the array, and we
/// can use that, combined with the number of nodes in the array, to figure out
/// if this is an internal or leaf node, and how many inputs it should have. We
/// are using a complete K-ary tree stored in an array which gives us that
/// closed form solution.
static int _get_children(_tree_t *tree, _tree_node_t *node) {
  return (node->children) ? node->children : K;
}

/// This is the entry point for the tree broadcast operation.
///
/// The reduced value is in the parcel buffer pointed to by the root's
/// parcels[0] parcel. This currently does a linear traversal of the leaf nodes
/// for the broadcast. These nodes are contiguous in the tree array, so we could
/// do this with a parallel for. We could also use to the tree structure to do a
/// parallel async spawn.
static int _tree_broadcast(_tree_t *tree) {
  const void *data = hpx_parcel_get_data(tree->nodes[0].parcels[0]);
  size_t bytes = tree->nodes[0].parcels[0]->size;
  char *value = malloc(bytes);
  dbg_assert(value);
  memcpy(value, data, bytes);

  // clear all of the the internal nodes, preserving the children count of the
  // last internal node
  if (tree->I) {
    int children = tree->nodes[tree->I - 1].children;
    // NB: missing synchronization on counts, is this okay?
    memset(tree->nodes, 0, sizeof(_tree_node_t) * tree->I);
    tree->nodes[tree->I - 1].children = children;
  }

  for (int i = tree->I, e = tree->N; i < e; ++i) {
    _tree_node_t *node = &tree->nodes[i];

    // reset the count at this node---this will let joins for the next
    // generation start to arrive, which is okay because input n can't try and
    // join before it gets the previous generation response
    sync_store(&node->count, 0, SYNC_RELEASE);
    for (int c = 0, e = _get_children(tree, node); c < e; ++c) {
      hpx_parcel_t *p = node->parcels[c];
      node->parcels[c] = NULL;
      dbg_assert(!p->ustack);
      memcpy(p->buffer, value, bytes);
      p->size = bytes;
      parcel_launch(p);
    }
  }
  return HPX_SUCCESS;
}

/// Perform a reduction on a node.
///
/// The value to reduce is stored in the parcel, @p p's data buffer, and @p id
/// is the child index that we are reducing from. This will recursively reduce
/// at the next tree level up, if this is the last child arriving at @p node. If
/// @p node is the root node, and this is the last child arriving, this will
/// cascade into a broadcast of the reduced value.
static int _reduce(_tree_t *tree, _tree_node_t *node, int c, hpx_parcel_t *p) {
  dbg_assert(p);
  dbg_assert(node->parcels[c] == NULL);
  node->parcels[c] = p;
  int n = sync_fadd(&node->count, 1, SYNC_ACQ_REL) + 1;
  int children = _get_children(tree, node);
  if (n < children) {
    return HPX_SUCCESS;
  }

  // perform the reduction into the data buffer for the first parcel
  hpx_action_t op = tree->op;
  for (int i = 1; i < children; ++i) {
    _op(op, node->parcels[0], node->parcels[i]);
  }

  // if this was the root, broadcast the result---have to replace whichever
  // continutation parcel we store that is "me" though, otherwise we could try
  // and send it concurrent with the broadcast loop---that would be bad
  _tree_node_t *parent = _get_parent(tree, node);
  if (!parent) {
    return _tree_broadcast(tree);
  }

  // figure out which child I am for my parent, and continue reducing
  int d = node - _get_child(tree, parent, 0);
  dbg_assert(node == _get_child(tree, parent, d));
  return _reduce(tree, parent, d, node->parcels[0]);
}

static void _tree_fini(lco_t *lco) {
  if (!lco) {
    return;
  }

  lco_lock(lco);
  lco_fini(lco);
}

static const lco_class_t _tree_vtable = {
  .on_fini     = _tree_fini,
  .on_error    = NULL,
  .on_set      = NULL,
  .on_attach   = NULL,
  .on_get      = NULL,
  .on_getref   = NULL,
  .on_release  = NULL,
  .on_wait     = NULL,
  .on_reset    = NULL,
  .on_size     = NULL
};

static int _tree_init_handler(_tree_t *tree, int n, int N, int I, hpx_action_t op) {
  lco_init(&tree->lco, &_tree_vtable);
  tree->op = op;
  tree->n = n;
  tree->N = N;
  tree->I = I;

  // the last leaf could be missing some children
  tree->nodes[N - 1].children = n % K;

  // the last "internal" node is also missing some children, we need to figure
  // out how many leaves we have at the maximum height
  double logK = log(K);
  double logN = log(N);
  int h = ceil(logN/logK);

  // do braindead integer exponent to figure out how many extra nodes we have
  int nodes[h];
  nodes[0] = 1;
  for (int i = 1, e = h; i < e; ++i) {
    nodes[i] = K * nodes[i - 1];
  }

  int full = 0;
  for (int i = 0, e = h; i < e; i++) {
    full += nodes[i];
  }

  // futre out how many extra nodes there are in level h
  int extra = N - full;
  tree->nodes[I - 1].children = extra % K;

  return HPX_SUCCESS;
}

hpx_addr_t hpx_lco_allreduce_new(size_t inputs, size_t outputs, size_t size,
                                 hpx_action_t id, hpx_action_t op) {
  dbg_assert(inputs == outputs);
  dbg_assert(!size || id);
  dbg_assert(!size || op);

  dbg_assert(inputs < UINT32_MAX/2);

  // How many leaves will I need?
  int L = ceil_div_32((int)inputs, K);

  // How many internal nodes will I need?
  // L = (K - 1) * I + 1
  // I = (L - 1) / (K - 1)
  int I = ceil_div_32(L - 1, K - 1);
  int N = L + I;

  size_t bytes = sizeof(_tree_t) + sizeof(_tree_node_t) * N;
  hpx_addr_t gva = hpx_gas_calloc_local(1, bytes, 0);
  LCO_LOG_NEW(gva);

  _tree_t *tree = NULL;
  if (!hpx_gas_try_pin(gva, (void**)&tree)) {
    dbg_error("could not allocate %zu bytes for an allreduce", bytes);
  }

  _tree_init_handler(tree, inputs, N, I, op);
  hpx_gas_unpin(gva);
  return gva;
}

/// This serves as the entry point for the join operation.
///
/// Joining uses a variable-length buffer because we don't know how large the
/// joined data is, and it also needs to send along the id of the input that we
/// are joining.
static int _join_handler(_tree_t *tree, _join_args_t *args, size_t n) {
  // Create a continuation parcel that "steals" the current continuation, and
  // serves as storage for the reduced value. The alignemnt of parcel data is
  // important---it should be at least 16-byte aligned so that we can use it to
  // reduce into, and it's important that we eagerly copy the value because it
  // will evaporate when we exit otherwise.
  size_t bytes = n - sizeof(*args);
  hpx_parcel_t *c = self->current;
  hpx_parcel_t *p = parcel_new(c->c_target, c->c_action, HPX_NULL,
                               HPX_ACTION_NULL, c->pid, NULL, bytes);
  memcpy(p->buffer, args->data, bytes);
  c->c_target = HPX_NULL;
  c->c_action = HPX_ACTION_NULL;

  int id = args->id;
  int leaf = tree->I + id / K;
  int child = id % K;
  return _reduce(tree,  &tree->nodes[leaf], child, p);
}
static HPX_ACTION(HPX_INTERRUPT, HPX_PINNED | HPX_MARSHALLED, _join,
                  _join_handler, HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

hpx_status_t hpx_lco_allreduce_join(hpx_addr_t lco, int id, size_t n,
                                    const void *value, hpx_action_t cont,
                                    hpx_addr_t at) {
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(_join_args_t) + n);
  dbg_assert(p);
  p->target = lco;
  p->action = _join;
  p->c_target = at;
  p->c_action = cont;

  _join_args_t *args = hpx_parcel_get_data(p);
  args->id = id;
  memcpy(args->data, value, n);

  parcel_launch(p);
  return HPX_SUCCESS;
}

hpx_status_t hpx_lco_allreduce_join_sync(hpx_addr_t lco, int id, size_t n,
                                         const void *value, void *out) {
  hpx_addr_t future = hpx_lco_future_new(n);
  dbg_assert(future);
  hpx_lco_allreduce_join(lco, id, n, value, hpx_lco_set_action, future);
  int e = hpx_lco_get(future, n, out);
  hpx_lco_delete(future, HPX_NULL);
  return e;
}

hpx_status_t hpx_lco_allreduce_join_async(hpx_addr_t lco, int id, size_t n,
                                          const void *value, void *out,
                                          hpx_addr_t done) {
  return HPX_ERROR;
}
