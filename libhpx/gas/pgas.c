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
#include "libhpx/gas.h"
#include "libhpx/scheduler.h"
#include "addr.h"
#include "sbrk.h"

/// ----------------------------------------------------------------------------
/// This may not be the most elegant way to deal with the global address space
/// data, but it's straightforward and relatively efficient.
/// ----------------------------------------------------------------------------

static void  **_btt = NULL;                     // the block translation table
static void *_local = NULL;                     // the local block

static SYNC_ATOMIC(uint32_t _next_byte);        // the next local byte offset

static hpx_action_t _alloc_blocks = 0;


static uint32_t _row(uint32_t block_id) {
  return block_id / hpx_get_num_ranks();
}


static uint32_t _owner(uint32_t block_id) {
  return block_id % hpx_get_num_ranks();
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
  uint32_t base_id = addr_block_id(target);
  int ranks = hpx_get_num_ranks();
  for (int i = 0; i < n; ++i) {
    uint32_t row = _row(base_id + i * ranks);
    sync_store(&_btt[row], &blocks[size * i], SYNC_RELEASE);
  }

  return HPX_SUCCESS;
}


static void _delete(gas_t *gas) {
  if (_local)
    munmap(_local, UINT32_MAX);
  if (_btt)
    munmap(_btt,UINT32_MAX * sizeof(_btt[0]));
}


/// ----------------------------------------------------------------------------
/// Local allocation is done from our designated block. Allocation is always
/// done to 8 byte alignment.
/// ----------------------------------------------------------------------------
static hpx_addr_t _alloc(size_t bytes, gas_t *gas) {
  bytes += bytes % 8;
  uint32_t offset = sync_fadd(&_next_byte, bytes, SYNC_ACQ_REL);
  if (UINT32_MAX - offset < bytes) {
    dbg_error("exhausted local allocation limit with %lu-byte allocation.\n",
              bytes);
    hpx_abort();
  }

  return hpx_addr_add(HPX_HERE, offset);
}


/// ----------------------------------------------------------------------------
/// This is currently trying to provide the layout:
///
/// shared [1] T foo[n]; where sizeof(T) == bytes
/// ----------------------------------------------------------------------------
static hpx_addr_t _global_alloc(size_t n, uint32_t bytes, gas_t *gas) {
  // For each locality, I need to broadcast an allocation request for the right
  // number of blocks.
  uint32_t base_id = gas_sbrk(n);
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


static int _where(const hpx_addr_t addr, gas_t *gas) {
  return _owner(addr_block_id(addr));
}


/// ----------------------------------------------------------------------------
/// PGAS doesn't pin memory, this just does the mod computation on address block
/// to see if it belongs to me, and then checks my table to see if there's a
/// mapping for it.
/// ----------------------------------------------------------------------------
static bool _try_pin(const hpx_addr_t addr, void **local, gas_t *gas) {
  int block_id = addr_block_id(addr);
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
static void _unpin(const hpx_addr_t addr, gas_t *gas) {
}


static void _init(const struct boot *boot) {
  gas_sbrk_init(boot_n_ranks(boot));

  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE | MAP_NONBLOCK;

  _local = mmap(NULL, UINT32_MAX, prot, flags, -1, 0);
  _btt = mmap(NULL, UINT32_MAX * sizeof(_btt[0]), prot, flags, -1, 0);
  _btt[0] = _local;

  // set the local block base
  sync_store(&_next_byte, 0, SYNC_RELEASE);

  // register the actions that we use
  _alloc_blocks = HPX_REGISTER_ACTION(_alloc_blocks_action);

  // update the HERE address
  HPX_HERE = HPX_THERE(boot_rank(boot));
}


/// ----------------------------------------------------------------------------
/// PGAS is a singleton
/// ----------------------------------------------------------------------------
static gas_t pgas = {
  .delete       = _delete,
  .alloc        = _alloc,
  .global_alloc = _global_alloc,
  .where        = _where,
  .try_pin      = _try_pin,
  .unpin        = _unpin
};


gas_t *gas_pgas_new(const struct boot *boot) {
  if (_local == NULL)
    _init(boot);

  return &pgas;
}

