// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


/// ----------------------------------------------------------------------------
/// Down-and-dirty PGAS implementation.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/scheduler.h"
#include "gas.h"


/// ----------------------------------------------------------------------------
/// Allocation is done on a per-block basis.
///
/// There are 2^32 blocks available, and the maximum size for a block is 2^32
/// bytes. Most blocks are smaller than this. Each block is backed by at least 1
/// page allocation. There are some reserved blocks.
///
/// In particular, each rank has a reserved block to deal with its local
/// allocations, i.e., block 0 belongs to rank 0, block 1 belongs to rank 1,
/// etc. Rank 0's block 0 also serves as the "NULL" block, that contains
/// HPX_NULL.
/// ----------------------------------------------------------------------------


/// Null doubles as rank 0's HPX_HERE.
const hpx_addr_t HPX_NULL = HPX_ADDR_INIT(0, 0, 0);


/// Updated in hpx_init(), the HPX_HERE (HPX_THERE) block is a max-bytes
/// block. This means that any reasonable address computation within a here or
/// there address will remain on the same locality.
hpx_addr_t HPX_HERE = HPX_ADDR_INIT(0, 0, UINT32_MAX);


/// Uses the well-known, low-order, block mappings to construct a "there
/// address."
hpx_addr_t HPX_THERE(int i) {
  hpx_addr_t addr = hpx_addr_init(0, i, UINT32_MAX);
  return addr;
}


static void **_btt;                             /// The block translation table.
static void *_local;                            /// The local block.


/// Right now, this action runs at rank 0, and simply allocates a bunch of
/// consecutive blocks for an allocation.
static hpx_action_t _gasbrk;
static hpx_action_t _alloc_blocks;
static SYNC_ATOMIC(uint32_t _next_block);
static SYNC_ATOMIC(uint32_t _next);


static uint32_t _block_id(hpx_addr_t addr) {
  assert(addr.block_bytes);
  return addr.base_id + (addr.offset / addr.block_bytes);
}


static uint32_t _row(uint32_t block_id) {
  return block_id / hpx_get_num_ranks();
}


static uint32_t _owner(uint32_t block_id) {
  return block_id % hpx_get_num_ranks();
}


/// The action that performs the global sbrk.
static int _gasbrk_action(size_t *args) {
  if (!hpx_addr_eq(HPX_HERE, HPX_THERE(0)))
    return dbg_error("Centralized allocation expects rank 0");

  // bump the next block id by the required number of blocks---always bump a
  // ranks-aligned value
  int ranks = hpx_get_num_ranks();
  size_t n = *args + (*args % ranks);
  int next = sync_fadd(&_next_block, n, SYNC_ACQ_REL);
  if (UINT32_MAX - next < n) {
    dbg_error("rank out of blocks for allocation size %lu\n", n);
    hpx_abort();
  }

  // return the base block id of the allocated blocks, the caller can use this
  // to initialize block addresses
  hpx_thread_continue(sizeof(next), &next);
}


/// The action that performs a global allocation for a rank.
static int _alloc_blocks_action(uint32_t *args) {
  uint32_t n = args[0];
  uint32_t size = args[1];

  // Blocks are all allocated contiguously, and densely, at the rank. Do the
  // malloc.
  uint64_t bytes = n * size;
  char *blocks = malloc(bytes);
  assert(blocks);

  // Insert all of the mappings (always block cyclic allocations).
  hpx_addr_t target = hpx_thread_current_target();
  uint32_t base_id = _block_id(target);
  int ranks = hpx_get_num_ranks();
  for (int i = 0; i < n; ++i) {
    uint32_t row = _row(base_id + i * ranks);
    sync_store(&_btt[row], &blocks[size * i], SYNC_RELEASE);
  }

  return HPX_SUCCESS;
}


void gas_init(const boot_t *boot) {
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE | MAP_NONBLOCK;

  // allocate the block translation table and the local region, and insert the
  // base mapping for that block---compacted because of PGAS
  _local = mmap(NULL, UINT32_MAX, prot, flags, -1, 0);
  _btt = mmap(NULL, UINT32_MAX * sizeof(_btt[0]), prot, flags, -1, 0);
  _btt[0] = _local;

  // initialize our two sbrk counters
  sync_store(&_next_block, boot_n_ranks(boot), SYNC_RELEASE);
  sync_store(&_next, 0, SYNC_RELEASE);

  // register the actions that we use
  _gasbrk = HPX_REGISTER_ACTION(_gasbrk_action);
  _alloc_blocks = HPX_REGISTER_ACTION(_alloc_blocks_action);

  // update the HERE address
  HPX_HERE = HPX_THERE(boot_rank(boot));
}


int gas_where(const hpx_addr_t addr) {
  return _owner(_block_id(addr));
}

/// ----------------------------------------------------------------------------
/// This is currently trying to provide the layout:
///
/// shared [1] T foo[n]; where sizeof(T) == bytes
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_global_alloc(size_t n, uint32_t bytes) {
  uint32_t base_id;

  // Start by getting the blocks that I need, _gasbrk returns the base block id
  // for the allocation, and ensures that at least the next "n" blocks are
  // available.
  hpx_addr_t f = hpx_lco_future_new(sizeof(base_id));
  hpx_call(HPX_THERE(0), _gasbrk, &n, sizeof(n), f);
  hpx_lco_get(f, &base_id, sizeof(base_id));
  hpx_lco_delete(f, HPX_NULL);

  // For each locality, I need to broadcast an allocation request for the right
  // number of blocks.
  int ranks = hpx_get_num_ranks();
  hpx_addr_t and = hpx_lco_and_new(ranks);
  uint32_t blocks_per_locality = n / ranks + ((n % ranks) ? 1 : 0);
  uint32_t args[2] = {
    blocks_per_locality,
    bytes
  };

  for (int i = 0; i < ranks; ++i) {
    hpx_addr_t addr = hpx_addr_init(0, base_id + i, bytes);
    hpx_call(addr, _alloc_blocks, &args, sizeof(args), and);
  }

  // The global alloc is currently synchronous, because the btt mappings aren't
  // complete until the allocation is complete.
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  // Return the base address.
  hpx_addr_t base_addr = hpx_addr_init(0, base_id, bytes);
  return base_addr;
}


/// ----------------------------------------------------------------------------
///
/// ----------------------------------------------------------------------------
bool
hpx_addr_eq(const hpx_addr_t lhs, const hpx_addr_t rhs) {
  // block_bytes are constant per-allocation-id
  return (lhs.offset == rhs.offset) && (lhs.base_id == rhs.base_id) &&
  (lhs.block_bytes == rhs.block_bytes);
}


/// ----------------------------------------------------------------------------
/// PGAS doesn't pin memory, this just does the mod computation on address block
/// to see if it belongs to me, and then checks my table to see if there's a
/// mapping for it.
/// ----------------------------------------------------------------------------
bool
hpx_addr_try_pin(const hpx_addr_t addr, void **local) {
  int block_id = _block_id(addr);
  uint32_t owner = _owner(block_id);
  if (owner != hpx_get_my_rank())
    return false;

  // If I own this, figure out where its mapping should be (dense table).
  uint32_t row = _row(block_id);
  void *base;
  sync_load(base, &_btt[row], SYNC_ACQUIRE);
  if (!base)
    return false;

  // If the programmer wants the mapping, output it, by offsetting from the base
  // as much as we need to.
  if (local)
    *local = ((char*)base + addr.offset % addr.block_bytes);

  return true;
}


/// ----------------------------------------------------------------------------
/// PGAS doesn't need to unpin, since it never pins, it just translates.
/// ----------------------------------------------------------------------------
void
hpx_addr_unpin(const hpx_addr_t addr) {
}


/// ----------------------------------------------------------------------------
/// Perform address arithmetic.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_addr_add(const hpx_addr_t addr, int bytes) {
  // not checking overflow
  uint64_t offset = addr.offset + bytes;
  hpx_addr_t result = hpx_addr_init(offset, addr.base_id, addr.block_bytes);
  return result;
}

/// ----------------------------------------------------------------------------
/// Local allocation is done from our designated block. Allocation is always
/// done to 8 byte alignment.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_alloc(size_t bytes) {
  bytes += bytes % 8;
  uint32_t offset = sync_fadd(&_next, bytes, SYNC_ACQ_REL);
  if (UINT32_MAX - offset < bytes) {
    dbg_error("exhausted local allocation limit with %lu-byte allocation.\n",
              bytes);
    hpx_abort();
  }

  return hpx_addr_add(HPX_HERE, offset);
}
