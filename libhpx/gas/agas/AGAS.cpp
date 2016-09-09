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

#include "AGAS.h"
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/events.h"
#include "libhpx/gpa.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/rebalancer.h"
#include "libhpx/Worker.h"                      // self->getCurrentParcel()
#include "libhpx/util/math.h"
#include <cstdlib>
#include <cstring>

namespace {
using libhpx::util::ceil_div;
using libhpx::util::ceil_log2;
using libhpx::gas::agas::AGAS;
using libhpx::gas::agas::ChunkAllocator;
using libhpx::gas::agas::GlobalVirtualAddress;

LIBHPX_ACTION(HPX_DEFAULT, 0, InsertTranslation, AGAS::InsertTranslationHandler,
              HPX_ADDR, HPX_UINT, HPX_SIZE_T, HPX_UINT32);
LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED | HPX_PINNED, UpsertBlock,
              AGAS::UpsertBlockHandler, HPX_POINTER, HPX_POINTER, HPX_SIZE_T);
LIBHPX_ACTION(HPX_DEFAULT, 0, InvalidateMapping, AGAS::InvalidateMappingHandler,
              HPX_ADDR, HPX_INT);
LIBHPX_ACTION(HPX_DEFAULT, 0, Move, AGAS::MoveHandler, HPX_ADDR);
LIBHPX_ACTION(HPX_DEFAULT, 0, FreeBlock, AGAS::FreeBlockHandler, HPX_ADDR);
LIBHPX_ACTION(HPX_DEFAULT, 0, FreeSegment, AGAS::FreeSegmentHandler, HPX_ADDR);
LIBHPX_ACTION(HPX_DEFAULT, 0, FreeAsync, AGAS::FreeAsyncHandler, HPX_ADDR);
LIBHPX_ACTION(HPX_DEFAULT, 0, InsertUserBlock, AGAS::InsertUserBlockHandler,
              HPX_ADDR, HPX_SIZE_T, HPX_SIZE_T, HPX_UINT32, HPX_POINTER,
              HPX_INT);
LIBHPX_ACTION(HPX_DEFAULT, 0,
              InsertCyclicSegment, AGAS::InsertCyclicSegmentHandler, HPX_ADDR,
              HPX_SIZE_T, HPX_SIZE_T, HPX_UINT32, HPX_POINTER, HPX_INT);
LIBHPX_ACTION(HPX_DEFAULT, 0, AllocateCyclic, AGAS::AllocateCyclicHandler,
              HPX_SIZE_T, HPX_SIZE_T, HPX_UINT32, HPX_UINT32, HPX_INT);
}

thread_local size_t AGAS::BlockSizePassthrough_;

AGAS::AGAS(const config_t* config, boot_t* boot)
    : btt_(0),
      chunks_(0),
      global_(chunks_, HEAP_SIZE),
      cyclic_(nullptr)
{
  initAllocators();
  rebalancer_init();

  if (here->rank == 0) {
    cyclic_ = new ChunkAllocator(chunks_, HEAP_SIZE);
  }

  GlobalVirtualAddress gva(there(here->rank));
  btt_.insert(gva, here->rank, here, 1, HPX_GAS_ATTR_NONE);
}

AGAS::~AGAS()
{
  delete cyclic_;
  rebalancer_finalize();
}

uint64_t
AGAS::maxBlockSize() const
{
  return uint64_t(1) << GPA_MAX_LG_BSIZE;
}

uint32_t
AGAS::ownerOf(hpx_addr_t addr) const
{
  GVA gva(addr);

  if (gva.offset == THERE_OFFSET) {
    return gva.home;
  }

  bool cached;
  uint32_t owner = btt_.getOwner(gva, cached);
  if (!cached) {
    EVENT_GAS_MISS(addr, owner);
  }
  dbg_assert(owner < here->ranks);
  return owner;
}

int
AGAS::upsertBlock(GVA src, uint32_t attr, size_t bsize, const char block[])
{
  void *lva = std::malloc(bsize);
  std::memcpy(lva, block, bsize);
  btt_.upsert(src, here->rank, lva, 1, attr);
  return HPX_SUCCESS;
}

int
AGAS::invalidateMapping(GVA dst, unsigned to)
{
  // unnecessary to move to the same locality
  if (here->rank == to) {
    return HPX_SUCCESS;
  }

  GVA src(hpx_thread_current_target());

  // instrument the move event
  EVENT_GAS_MOVE(src, HPX_HERE, dst);

  if (btt_.getOwner(src) != here->rank) {
    return HPX_RESEND;
  }

  size_t bsize = src.getBlockSize();
  std::unique_ptr<UpsertBlockArgs> args(new(bsize) UpsertBlockArgs());
  args->src = src;
  void* lva = btt_.move(src, to, args->attr);
  std::memcpy(args->block, lva, bsize);
  if (src.home != here->rank) {
    std::free(lva);
  }

  // @todo We could explicitly send a continuation parcel to avoid a
  //       malloc/memcpy/free here.
  size_t bytes = sizeof(UpsertBlockArgs) + bsize;
  void* buffer = args.get();
  return hpx_call_cc(dst, UpsertBlock, buffer, bytes);
}

int
AGAS::MoveHandler(hpx_addr_t src)
{
  hpx_addr_t dst = hpx_thread_current_target();
  return hpx_call_cc(src, InvalidateMapping, &dst, &here->rank);
}

void
AGAS::move(hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync)
{
  bool found;
  uint32_t owner = btt_.getOwner(dst, found);
  if (found) {
    hpx_call(src, InvalidateMapping, sync, &dst, &owner);
  }
  else {
    hpx_call(dst, Move, sync, &src);
  }
}

void
AGAS::free(hpx_addr_t addr, hpx_addr_t rsync) {
  if (addr == HPX_NULL) {
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }
  else if (void* lva = btt_.tryRemoveForFastpathFree(addr)) {
    global_free(lva);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }
  else {
    dbg_check( hpx_call(addr, FreeAsync, rsync, &addr) );
  }
}

int
AGAS::freeBlock(GVA gva)
{
  void *lva = btt_.remove(gva);
  dbg_assert(lva);

  if (gva.home == here->rank) {
    return HPX_SUCCESS;
  }

  // This was a relocated block, free the clone and then forward the request
  // back to the home locality.
  std::free(lva);
  hpx_addr_t block = gva;
  return hpx_call_cc(block, FreeBlock, &block);
}

void
AGAS::freeSegment(GVA gva)
{
  // This message is always sent to the right place, but the base address isn't
  // always correct. This happens because we are using this action to deal with
  // segments related to cyclic allocation in addition to segments related to
  // normal allocation. The cyclic allocation case has broadcast the base
  // segment's address, so we patch up the address to get the "right" segment.
  gva.home = here->rank;

  // We need to know both the number of blocks in the segment, and the backing
  // virtual address of the segment. The number of blocks allows us to iterate
  // through them, and the backing virtual address will allow us to free the
  // backing memory for cyclic segments.
  size_t blocks;
  void* lva = btt_.getSegment(gva, blocks);
  size_t bsize = gva.getBlockSize();
  hpx_addr_t lco = hpx_lco_and_new(blocks);
  for (int i = 0, e = blocks; i < e; ++i) {
    // all blocks in a segment are contiguous so we can use local add here.
    hpx_addr_t block = gva.add(i * bsize, bsize);
    dbg_check( hpx_call(block, FreeBlock, lco, &block) );
  }
  dbg_check( hpx_lco_wait(lco) );
  hpx_lco_delete(lco, HPX_NULL);

  // We need to release the memory backing the segment if it is part of a cyclic
  // allocation and is not the rank 0 segment, which is dealt with by the
  // FreeAsync handler. These asymmetries are a result of the way that we do all
  // cyclic memory management at rank 0.
  if (gva.cyclic && here->rank != 0) {
    std::free(lva);
  }
}

void
AGAS::freeAsync(GVA gva)
{
  dbg_assert(gva.home == here->rank);

  // We need to free this after everything has been cleaned up.
  void *lva = btt_.getLVA(gva);

  // Cyclic allocations have segments at each rank, so we broadcast the command
  // to clean up the segment. Otherwise we just clean up the local segment. We
  // optimize here for single-block allocations by skipping the segment code.
  if (gva.cyclic) {
    dbg_check( hpx_bcast_rsync(FreeSegment, &gva) );
    cyclic_free(lva);
  }
  else if (btt_.getBlocks(gva) == 1) {
    dbg_check( hpx_call_sync(gva, FreeBlock, NULL, 0, &gva) );
    global_free(lva);
  }
  else {
    dbg_check( hpx_call_sync(HPX_HERE, FreeSegment, NULL, 0, &gva) );
    global_free(lva);
  }
}

GlobalVirtualAddress
AGAS::lvaToGVA(const void *lva, size_t bsize) const
{
  dbg_assert(bsize <= maxBlockSize());
  uint64_t offset = chunks_.offsetOf(lva);
  return GVA(offset, ceil_log2(bsize), 0, here->rank);
}

void
AGAS::insertTranslation(GVA gva, unsigned rank, size_t blocks, uint32_t attr)
{
  btt_.upsert(gva, rank, nullptr, blocks, attr);
}

int
AGAS::insertUserBlock(GVA gva, size_t blocks, size_t padded, uint32_t attr,
                      void *lva, int zero)
{
  dbg_assert(gva);
  dbg_assert(blocks);
  dbg_assert(padded);
  dbg_assert(lva);

  const hpx_parcel_t *p = self->getCurrentParcel();

  // If the block isn't being inserted at the source of the allocation request
  // then it's basically a moved block, and we need to allocate backing space
  // for it locally.
  if (p->src != here->rank) {
    size_t boundary = std::max(size_t(8u), padded);
    if (posix_memalign(&lva, boundary, padded)) {
      dbg_error("Failed memalign\n");
    }
  }

  // Zero the memory if necessary.
  if (zero) {
    std::memset(lva, 0, padded);
  }

  // Insert the translation
  btt_.upsert(gva, here->rank, lva, blocks, attr);

  // And push the translation back to the source locality if we're
  // out-of-place from the initial call.
  if (p->src != here->rank) {
    hpx_addr_t src = HPX_THERE(p->src);
    return hpx_call_cc(src, InsertTranslation, &gva, &here->rank, &blocks,
                       &attr);
  }
  else {
    return HPX_SUCCESS;
  }
}

void
AGAS::insertCyclicSegment(GVA gva, size_t blocks, size_t padded, uint32_t attr,
                          void *lva, int zero)
{
  dbg_assert(gva);
  dbg_assert(blocks);
  dbg_assert(padded);
  dbg_assert(lva);

  size_t bytes = blocks * padded;
  if (here->rank != 0) {
    size_t boundary = std::max(size_t(8u), padded);
    if (posix_memalign(&lva, boundary, bytes)) {
      dbg_error("Failed memalign\n");
    }
  }

  if (zero) {
    std::memset(lva, 0, bytes);
  }

  // Adjust the gva to point at the first block on this rank.
  gva = gva.add(here->rank * padded, padded);

  // And insert translations for all of the blocks.
  for (unsigned i = 0; i < blocks; ++i) {
    btt_.insert(gva, here->rank, lva, blocks, attr);
    gva = gva.add(here->ranks * padded, padded);
    lva = static_cast<char*>(lva) + padded;
  }
}

/// @todo why are we ignoring boundary here?
GlobalVirtualAddress
AGAS::allocateUser(size_t n, size_t bsize, uint32_t, hpx_gas_dist_t dist,
                   uint32_t attr, int zero)
{
  dbg_assert(bsize <= maxBlockSize());

  // determine the padded block size
  size_t padded = uintptr_t(1) << ceil_log2(bsize);

  // Out of band communication to the chunk allocator infrastructure.
  BlockSizePassthrough_ = padded;

  // Allocate the blocks as a contiguous, aligned array from local memory.
  void *local = global_memalign(padded, n * padded);
  if (!local) {
    dbg_error("failed user-defined allocation\n");
  }

  // Iterate through all of the blocks and insert them into the block
  // translation tables. We use an asynchronous interface here in order to mask
  // the difference between local blocks and global blocks.
  GVA gva = lvaToGVA(local, padded);
  hpx_addr_t done = hpx_lco_and_new(n);
  for (unsigned i = 0; i < n; ++i) {
    hpx_addr_t where = (i == 0) ? HPX_HERE : dist(i, n, bsize);
    GVA addr = gva.add(i * padded, padded);
    const void* lva = static_cast<char*>(local) + i * padded;
    hpx_call(where, InsertUserBlock, done, &addr, &n, &padded, &attr, &lva,
             &zero);
  }
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  return gva;
}

GlobalVirtualAddress
AGAS::allocateLocal(size_t n, size_t bsize, uint32_t boundary, uint32_t attr,
                    bool zero)
{
  dbg_assert(bsize < maxBlockSize());

  // use the local allocator to get some memory that is part of the global
  // address space
  uint32_t padded = uintptr_t(1) << ceil_log2(bsize);
  uint32_t aligned = max_u32(boundary, padded);

  // Out of band communication to the chunk allocator infrastructure.
  BlockSizePassthrough_ = aligned;

  void *lva = global_memalign(aligned, n * padded);
  if (!lva) {
    return HPX_NULL;
  }

  if (zero) {
    std::memset(lva, 0, n * padded);
  }

  GVA gva = lvaToGVA(lva, padded);
  for (unsigned i = 0; i < n; ++i) {
    GVA block = gva.add(i * padded, padded);
    void* local = static_cast<char*>(lva) + i * padded;
    btt_.insert(block, here->rank, local, n, attr);
  }
  return gva;
}

/// @todo: why do we ignore boundary?
GlobalVirtualAddress
AGAS::allocateCyclic(size_t n, size_t bsize, uint32_t, uint32_t attr, int zero)
{
  dbg_assert(here->rank == 0);
  dbg_assert(bsize <= maxBlockSize());

  // Figure out how many blocks per node we need, and what the size is.
  auto blocks = ceil_div(n, size_t(here->ranks));
  auto  align = ceil_log2(bsize);
  auto padded = size_t(1u) << align;

  // Transmit the padded block size through to the chunk allocator.
  BlockSizePassthrough_ = padded;

  // Allocate the blocks as a contiguous, aligned array from cyclic memory. This
  // only happens at rank 0, where the cyclic_memalign is functional. We use
  // this segment to define the GVA of the allocation, and then broadcast a
  // request for the entire system to allocate their own segment.
  void *lva = cyclic_memalign(padded, blocks * padded);
  if (!lva) {
    dbg_error("failed cyclic allocation\n");
  }

  GVA gva = lvaToGVA(lva, padded);
  gva.cyclic = 1;
  if (hpx_bcast_rsync(InsertCyclicSegment, &gva, &blocks, &padded, &attr, &lva,
                      &zero)) {
    dbg_error("failed to insert btt entries.\n");
  }
  return gva;
}

hpx_addr_t
AGAS::alloc_cyclic(size_t n, size_t bsize, uint32_t boundary, uint32_t attr) {
  int zero = 0;
  GVA gva;
  if (hpx_call_sync(HPX_THERE(0), AllocateCyclic, &gva, sizeof(gva), &n, &bsize,
                    &boundary, &attr, &zero)) {
    dbg_error("Failed to allocate.\n");
  }
  return gva;
}

hpx_addr_t
AGAS::calloc_cyclic(size_t n, size_t bsize, uint32_t boundary, uint32_t attr) {
  int zero = 1;
  GVA gva;
  if (hpx_call_sync(HPX_THERE(0), AllocateCyclic, &gva, sizeof(gva), &n, &bsize,
                    &boundary, &attr, &zero)) {
    dbg_error("Failed to allocate.\n");
  }
  return gva;
}

void
AGAS::memput(hpx_addr_t to, const void *from, size_t n, hpx_addr_t lsync,
             hpx_addr_t rsync)
{
  void *lto;
  if (!n) {
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }
  else if (!tryPin(to, &lto)) {
    here->net->memput(to, from, n, lsync, rsync);
  }
  else {
    std::memcpy(lto, from, n);
    unpin(to);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }
}

void
AGAS::memput(hpx_addr_t to, const void *from, size_t n, hpx_addr_t rsync)
{
  void *lto;
  if (!n) {
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }
  else if (!tryPin(to, &lto)) {
    here->net->memput(to, from, n, rsync);
  }
  else {
    std::memcpy(lto, from, n);
    unpin(to);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }
}

void
AGAS::memput(hpx_addr_t to, const void *from, size_t n) {
  void *lto;
  if (!n) {
  }
  else if (!tryPin(to, &lto)) {
    here->net->memput(to, from, n);
  }
  else {
    std::memcpy(lto, from, n);
    unpin(to);
  }
}

void
AGAS::memget(void *to, hpx_addr_t from, size_t n, hpx_addr_t lsync,
             hpx_addr_t rsync)
{
  void *lfrom;
  if (!n) {
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  } else if (!tryPin(from, &lfrom)) {
    here->net->memget(to, from, n, lsync, rsync);
  }
  else {
    std::memcpy(to, lfrom, n);
    unpin(from);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }
}

void
AGAS::memget(void *to, hpx_addr_t from, size_t n, hpx_addr_t lsync)
{
  void *lfrom;
  if (!n) {
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
  }
  else if (!tryPin(from, &lfrom)) {
    here->net->memget(to, from, n, lsync);
  }
  else {
    std::memcpy(to, lfrom, n);
    unpin(from);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
  }
}

void
AGAS::memget(void *to, hpx_addr_t from, size_t n)
{
  void *lfrom;
  if (!n) {
  }
  else if (!tryPin(from, &lfrom)) {
    here->net->memget(to, from, n);
  }
  else {
    std::memcpy(to, lfrom, n);
    unpin(from);
  }
}

void
AGAS::memcpy(hpx_addr_t to, hpx_addr_t from, size_t size, hpx_addr_t sync)
{
  void *lto;
  void *lfrom;

  if (!size) {
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
  }
  else if (!tryPin(to, &lto)) {
    here->net->memcpy(to, from, size, sync);
  }
  else if (!tryPin(from, &lfrom)) {
    unpin(to);
    here->net->memcpy(to, from, size, sync);
  }
  else {
    std::memcpy(lto, lfrom, size);
    hpx_gas_unpin(to);
    hpx_gas_unpin(from);
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
  }
}

void
AGAS::memcpy(hpx_addr_t to, hpx_addr_t from, size_t size)
{
  if (size) {
    hpx_addr_t sync = hpx_lco_future_new(0);
    memcpy(to, from, size, sync);
    dbg_check(hpx_lco_wait(sync));
    hpx_lco_delete(sync, HPX_NULL);
  }
}

void*
AGAS::chunkAllocate(void* addr, size_t n, size_t align, bool cyclic)
{
  if (cyclic) {
    dbg_assert(cyclic_);
    return cyclic_->allocate(addr, n, align, BlockSizePassthrough_);
  }
  else {
    return global_.allocate(addr, n, align, BlockSizePassthrough_);
  }
}

void
AGAS::chunkDeallocate(void* addr, size_t n, bool cyclic)
{
  if (cyclic) {
    dbg_assert(cyclic_);
    cyclic_->deallocate(addr, n);
  }
  else {
    global_.deallocate(addr, n);
  }
}
