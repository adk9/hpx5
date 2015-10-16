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
#include "process_collective_continuation.h"

/// The maximum tree arity is set based on the number of pointers we can fit in
/// a cacheline along with the node metadata.
/// @{
#define K ((HPX_CACHELINE_SIZE - 2 * sizeof(int)) / sizeof(void*))
/// @}

/// The join action is a marshaled action---this is its argument type.
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
/// Each tree node takes up one cacheline and stores up to K values to
/// reduce. Tree nodes are always allocated in arrays, so we can figure out the
/// index of the node using pointer arithmetic, and using the index we can
/// compute its parents and children.
typedef struct {
  int             count;
  int          children;
  hpx_parcel_t *parcels[K];
} _tree_node_t;

_HPX_ASSERT(sizeof(_tree_node_t) == HPX_CACHELINE_SIZE, _tree_node_unexpected_size);

/// The tree structure stores some global information about the tree including
/// the reduction callback, the size of the value being reduced, and the array
/// of nodes.
typedef struct {
  hpx_addr_t bcast;                             // the broadcast tree
  int            n;                             // the number of leaf nodes
  int            N;                             // the total number of nodes
  int            I;                             // the number of internal nodes
  hpx_action_t  op;                             // the reduction operation
  PAD_TO_CACHELINE(sizeof(int) * 3 + sizeof(hpx_action_t));
  _tree_node_t nodes[];                         // the array of tree nodes
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

/// Get the ith child of the node.
///
/// This does some index and pointer arithmetic to figure out the address of the
/// node's ith child. This is possible because the tree is stored as an array,
/// we can compute the node's index in the array, and the child(c) relation has
/// a closed form for a K-ary tree stored in an array.
static _tree_node_t *_get_child(_tree_t *tree, _tree_node_t *node, int c) {
  int i = node - tree->nodes;
  int j = K * i + 1 + c;
  dbg_assert(0 <= j && j < tree->N);
  return &tree->nodes[j];
}

/// Figure out how many inputs (i.e., children), a node has.
///
/// The node actually stores its number of children, except that for convenience
/// 0 means K children. This function performs that adjustment.
static int _get_children(const _tree_node_t *node) {
  return (node->children) ? node->children : K;
}

/// This is the entry point for the tree broadcast operation.
///
/// The reduced value is in the parcel buffer pointed to by the root's
/// parcels[0] parcel. This currently does a linear traversal of the leaf nodes
/// for the broadcast. These nodes are contiguous in the tree array, so we could
/// do this with a parallel for. We could also use to the tree structure to do a
/// parallel async spawn.
// static int _tree_broadcast(_tree_t *tree) {
//   const void *data = hpx_parcel_get_data(tree->nodes[0].parcels[0]);
//   size_t bytes = tree->nodes[0].parcels[0]->size;
//   char *value = malloc(bytes);
//   dbg_assert(value);
//   memcpy(value, data, bytes);

//   // clear all of the the internal nodes, preserving the children count of the
//   // last internal node
//   if (tree->I) {
//     int children = tree->nodes[tree->I - 1].children;
//     // NB: missing synchronization on counts, is this okay?
//     memset(tree->nodes, 0, sizeof(_tree_node_t) * tree->I);
//     tree->nodes[tree->I - 1].children = children;
//   }

//   for (int i = tree->I, e = tree->N; i < e; ++i) {
//     _tree_node_t *node = &tree->nodes[i];

//     // reset the count at this node---this will let joins for the next
//     // generation start to arrive, which is okay because input n can't try and
//     // join before it gets the previous generation response
//     sync_store(&node->count, 0, SYNC_RELEASE);
//     for (int c = 0, e = _get_children(node); c < e; ++c) {
//       hpx_parcel_t *p = node->parcels[c];
//       node->parcels[c] = NULL;
//       dbg_assert(!p->ustack);
//       memcpy(p->buffer, value, bytes);
//       p->size = bytes;
//       parcel_launch(p);
//     }
//   }
//   return HPX_SUCCESS;
// }

/// Perform a reduction on a node.
///
/// The value to reduce is stored in the parcel, @p p's data buffer, and @p c is
/// the child index that we are reducing from. This will recursively reduce at
/// @p node's parent, if this is the last child arriving at @p node. If @p node
/// is the root node, and this is the last child arriving, this will cascade
/// into a broadcast of the reduced value.
static int _reduce(_tree_t *tree, _tree_node_t *node, int c, hpx_parcel_t *p) {
  dbg_assert(p);
  dbg_assert(node->parcels[c] == NULL);

  // record the incoming value as the contribution from the @p c child, and see
  // if this was the last value for this node
  node->parcels[c] = p;
  int n = sync_fadd(&node->count, 1, SYNC_ACQ_REL) + 1;
  int children = _get_children(node);
  if (n < children) {
    return HPX_SUCCESS;
  }

  // perform the reduction into the data buffer for the 0th child for this node,
  // freeing the parcels as we go
  hpx_action_t op = tree->op;
  p = node->parcels[0];
  node->parcels[0] = NULL;
  for (int i = 1; i < children; ++i) {
    hpx_parcel_t *q = node->parcels[i];
    node->parcels[i] = NULL;
    _op(op, p, q);
    hpx_parcel_release(q);
  }

  // reset node count for reuse
  sync_store(&node->count, 0, SYNC_RELEASE);

  // if this was the root, broadcast the result
  _tree_node_t *parent = _get_parent(tree, node);
  if (!parent) {
    const void *data = hpx_parcel_get_data(p);
    size_t bytes = p->size;
    hpx_addr_t bcast = tree->bcast;
    int e = process_collective_continuation_set_lsync(bcast, bytes, data);
    hpx_parcel_release(p);
    return e;
  }

  // figure out which child I am for my parent, and continue reducing
  int d = node - _get_child(tree, parent, 0);
  dbg_assert(node == _get_child(tree, parent, d));
  return _reduce(tree, parent, d, p);
}

/// Initialize the local tree object.
static int _tree_init_handler(_tree_t *tree, int n, int N, int I,
                              hpx_action_t op, hpx_addr_t bcast) {

  tree->bcast = bcast;
  tree->n = n;
  tree->N = N;
  tree->I = I;
  tree->op = op;

  // the last leaf could be missing some children
  tree->nodes[N - 1].children = n % K;

  // the last "internal" node is also missing some children, we need to figure
  // out how many leaves we have at the maximum height
  double logK = log(K);
  double logN = log(N);
  int h = ceil(logN/logK);

  // do braindead integer exponent to figure out how many extra nodes we have
  dbg_assert(hpx_thread_can_alloca(h * sizeof(int)) > 0);
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

/// Create a local allreduce tree.
///
/// We will allocate a tree reduction LCO at the locality that is calling this
/// routine. In addition, we will allocate a cyclic broadcast array that will
/// manage the broadcast of the result of the reduction.
hpx_addr_t hpx_process_collective_allreduce_new(size_t size, int inputs,
                                                hpx_action_t op) {
  // How many leaves will I need?
  int L = ceil_div_32(inputs, K);

  // How many internal nodes will I need?
  // L = (K - 1) * I + 1
  // I = (L - 1) / (K - 1)
  int I = ceil_div_32(L - 1, K - 1);
  int N = L + I;

  // Allocate enough aligned bytes.
  size_t bytes = sizeof(_tree_t) + sizeof(_tree_node_t) * N;
  dbg_assert(bytes < UINT32_MAX);
  hpx_addr_t gva = hpx_gas_calloc_local(1, bytes, HPX_CACHELINE_SIZE);

  _tree_t *tree = NULL;
  if (!hpx_gas_try_pin(gva, (void**)&tree)) {
    dbg_error("could not allocate an allreduce\n");
  }
  // verify the tree alignment---this is important for scalability
  dbg_assert(((uintptr_t)tree & (HPX_CACHELINE_SIZE - 1)) == 0);

  // Allocate the distributed continuation
  hpx_addr_t bcast = process_collective_continuation_new(size, gva);

  // initialize the reduce tree
  _tree_init_handler(tree, inputs, N, I, op, bcast);
  hpx_gas_unpin(gva);

  // the client actually gets back the distributed continuation
  return bcast;
}

/// This serves as the entry point for the asynchronous join operation.
///
/// Joining uses a variable-length buffer because we don't know how large the
/// joined data is, and it also needs to send along the id of the input that we
/// are joining.
static int _join_handler(_tree_t *tree, _join_args_t *args, size_t n) {
  // Create a continuation parcel that "steals" the current continuation, and
  // serves as storage for the reduced value. The alignment of parcel data is
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

  // Figure out which node this input maps to, and start reducing.
  int id = args->id;
  int leaf = tree->I + id / K;
  int child = id % K;
  return _reduce(tree,  &tree->nodes[leaf], child, p);
}
static HPX_ACTION(HPX_INTERRUPT, HPX_PINNED | HPX_MARSHALLED, _join,
                  _join_handler, HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

/// The asynchronous join operation.
///
/// This will asynchronously call the continuation action with the reduced data
/// once the reduction is complete.
int hpx_process_collective_allreduce_join(hpx_addr_t target,
                                          int id, size_t bytes, const void *in,
                                          hpx_action_t c_action,
                                          hpx_addr_t c_target) {
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(_join_args_t) + bytes);
  dbg_assert(p);
  p->target = process_collective_continuation_append(target, bytes,
                                                     c_action, c_target);
  p->action = _join;
  p->c_target = c_target;
  p->c_action = c_action;

  _join_args_t *args = hpx_parcel_get_data(p);
  args->id = id;
  memcpy(args->data, in, bytes);

  parcel_launch(p);
  return HPX_SUCCESS;
}

/// The synchronous join operation.
///
/// This currently just forwards to the asynchronous version and waits.
int hpx_process_collective_allreduce_join_sync(hpx_addr_t target,
                                               int id, size_t bytes,
                                               const void *in, void *out) {
  hpx_addr_t future = hpx_lco_future_new(bytes);
  dbg_assert(future);
  hpx_process_collective_allreduce_join(target, id, bytes, in,
                                        hpx_lco_set_action, future);
  int e = hpx_lco_get(future, bytes, out);
  hpx_lco_delete(future, HPX_NULL);
  return e;
}

/// Delete an allreduce.
void hpx_process_collective_allreduce_delete(hpx_addr_t target) {
}
