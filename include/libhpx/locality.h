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
#ifndef LIBHPX_LOCALITY_H
#define LIBHPX_LOCALITY_H

/// ----------------------------------------------------------------------------
/// Exports all of the resources available at an HPX locality.
/// ----------------------------------------------------------------------------

#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"

struct boot_class;
struct btt_class;
struct network_class;
struct scheduler;
struct transport_class;
struct hpx_config;

typedef struct {
  hpx_locality_t               rank;            // this locality's rank
  int                         ranks;            // the total number of ranks
  struct boot_class           *boot;            // the bootstrap object
  struct gas_class             *gas;            // the global address space
  // struct btt_class             *btt;            // the block translation table
  struct transport_class *transport;            // the byte transport
  struct network_class     *network;            // the parcel transport
  struct scheduler           *sched;            // the scheduler data

  volatile uint32_t   local_sbrk;            // the local memory block sbrk
  volatile uint32_t  global_sbrk;            // the global block id sbrk
  volatile uint32_t     pvt_sbrk;            // the global private block sbrk
} locality_t;


/// Actions for use with HPX_THERE()
HPX_INTERNAL extern hpx_action_t locality_shutdown;
HPX_INTERNAL extern hpx_action_t locality_global_sbrk;
HPX_INTERNAL extern hpx_action_t locality_alloc_blocks;
typedef struct {
  hpx_action_t action;
  hpx_status_t status;
  char data[];
} locality_cont_args_t;
HPX_INTERNAL extern hpx_action_t locality_call_continuation;

HPX_INTERNAL extern hpx_action_t locality_gas_alloc;
HPX_INTERNAL extern hpx_action_t locality_gas_acquire;
HPX_INTERNAL extern hpx_action_t locality_gas_move;
typedef struct {
  hpx_addr_t addr;
  uint32_t rank;
} locality_gas_forward_args_t;
HPX_INTERNAL extern hpx_action_t locality_gas_forward;

/// The global locality is exposed through this "here" pointer.
///
/// The value of the pointer is equivalent to hpx_addr_try_pin(HPX_HERE, &here);
HPX_INTERNAL extern locality_t *here;

inline static bool lva_is_global(void *addr) {
  return here->gas->is_global(here->gas, addr);
}

inline static void *global_malloc(size_t bytes) {
  void *addr = here->gas->global.malloc(bytes);

  DEBUG_IF (!lva_is_global(addr))
    dbg_error("pgas: global malloc returned local pointer %p.\n", addr);

  return addr;
}

inline static void global_free(void *ptr) {
  DEBUG_IF (!lva_is_global(ptr))
    dbg_error("pgas: global free called on local pointer %p.\n", ptr);

  here->gas->global.free(ptr);
}

inline static void *global_calloc(size_t nmemb, size_t size) {
  void *addr = here->gas->global.calloc(nmemb, size);
  DEBUG_IF (!lva_is_global(addr))
    dbg_error("pgas: global calloc returned local pointer %p.\n", addr);

  return addr;
}

inline static void *global_realloc(void *ptr, size_t size) {
  DEBUG_IF (ptr && !lva_is_global(ptr))
    dbg_error("pgas: global realloc called on local pointer %p.\n", ptr);

  void *addr = here->gas->global.realloc(ptr, size);

  DEBUG_IF (!lva_is_global(addr))
    dbg_error("pgas: global realloc returned local pointer %p.\n", addr);

  return addr;
}

inline static void *global_valloc(size_t size) {
  void *addr = here->gas->global.valloc(size);

  DEBUG_IF (!lva_is_global(addr))
    dbg_error("pgas: global valloc returned local pointer %p.\n", addr);

  return addr;
}

inline static void *global_memalign(size_t boundary, size_t size) {
  void *addr = here->gas->global.memalign(boundary, size);

  DEBUG_IF (!lva_is_global(addr))
    dbg_error("pgas: global memalign returned local pointer %p.\n", addr);

  return addr;
}

inline static int global_posix_memalign(void **memptr, size_t alignment,
                                        size_t size) {
  int e = here->gas->global.posix_memalign(memptr, alignment, size);

  DEBUG_IF (!e && !lva_is_global(*memptr))
    dbg_error("pgas: global posix memalign returned local pointer %p.\n", *memptr);

  return e;
}

inline static void *local_malloc(size_t bytes) {
  void *addr = here->gas->local.malloc(bytes);

  DEBUG_IF (lva_is_global(addr))
    dbg_error("pgas: local malloc returned global pointer %p.\n", addr);

  return addr;
}

inline static void local_free(void *ptr) {
  DEBUG_IF (lva_is_global(ptr))
    dbg_error("pgas: local free passed global pointer %p.\n", ptr);

  here->gas->local.free(ptr);
}

inline static void *local_calloc(size_t nmemb, size_t size) {
  void *addr = here->gas->local.calloc(nmemb, size);

  DEBUG_IF (lva_is_global(addr))
    dbg_error("pgas: local calloc returned global pointer %p.\n", addr);

  return addr;
}

inline static void *local_realloc(void *ptr, size_t size) {
  DEBUG_IF (lva_is_global(ptr))
    dbg_error("pgas: local realloc called on global pointer %p.\n", ptr);

  void *addr = here->gas->local.realloc(ptr, size);

  DEBUG_IF (lva_is_global(addr))
    dbg_error("pgas: local realloc returned global pointer %p.\n", addr);

  return addr;
}

inline static void *local_valloc(size_t size) {
  void *addr =  here->gas->local.valloc(size);

  DEBUG_IF (lva_is_global(addr))
    dbg_error("pgas: local valloc returned global pointer %p.\n", addr);

  return addr;

}

inline static void *local_memalign(size_t boundary, size_t size) {
  void *addr = here->gas->local.memalign(boundary, size);

  DEBUG_IF (lva_is_global(addr))
    dbg_error("pgas: local memalign returned global pointer %p.\n", addr);

  return addr;
}

inline static int local_posix_memalign(void **memptr, size_t alignment,
                                        size_t size) {
  int e = here->gas->local.posix_memalign(memptr, alignment, size);

  DEBUG_IF (!e && lva_is_global(*memptr))
    dbg_error("pgas: local posix memalign returned global pointer %p.\n", *memptr);

  return e;
}


inline static hpx_addr_t lva_to_gva(void *lva) {
  assert(here && here->gas && here->gas->lva_to_gva);
  return here->gas->lva_to_gva(lva);
}


/// Allocate immovable local memory that is addressable globally.
HPX_INTERNAL hpx_addr_t locality_malloc(size_t bytes);


#endif // LIBHPX_LOCALITY_H
