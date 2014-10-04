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
struct network_class;
struct scheduler;
struct transport_class;
struct hpx_config;

typedef struct {
  hpx_locality_t               rank;            // this locality's rank
  int                         ranks;            // the total number of ranks
  struct boot_class           *boot;            // the bootstrap object
  struct gas_class             *gas;            // the global address space
  struct transport_class *transport;            // the byte transport
  struct network_class     *network;            // the parcel transport
  struct scheduler           *sched;            // the scheduler data
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
  dbg_assert(here && here->gas && here->gas->is_global);
  return here->gas->is_global(here->gas, addr);
}

inline static void *global_malloc(size_t bytes) {
  dbg_assert(here && here->gas && here->gas->global.malloc);
  return here->gas->global.malloc(bytes);
}

inline static void global_free(void *ptr) {
  dbg_assert(here && here->gas && here->gas->global.free);
  here->gas->global.free(ptr);
}

inline static void *global_calloc(size_t nmemb, size_t size) {
  dbg_assert(here && here->gas && here->gas->global.calloc);
  return here->gas->global.calloc(nmemb, size);
}

inline static void *global_realloc(void *ptr, size_t size) {
  dbg_assert(here && here->gas && here->gas->global.realloc);
  return here->gas->global.realloc(ptr, size);
}

inline static void *global_valloc(size_t size) {
  dbg_assert(here && here->gas && here->gas->global.valloc);
  return here->gas->global.valloc(size);
}

inline static void *global_memalign(size_t boundary, size_t size) {
  dbg_assert(here && here->gas && here->gas->global.memalign);
  return here->gas->global.memalign(boundary, size);
}

inline static int global_posix_memalign(void **memptr, size_t alignment,
                                        size_t size) {
  dbg_assert(here && here->gas && here->gas->global.posix_memalign);
  return here->gas->global.posix_memalign(memptr, alignment, size);
}

inline static void *local_malloc(size_t bytes) {
  dbg_assert(here && here->gas && here->gas->local.malloc);
  return here->gas->local.malloc(bytes);
}

inline static void local_free(void *ptr) {
  dbg_assert(here && here->gas && here->gas->local.free);
  here->gas->local.free(ptr);
}

inline static void *local_calloc(size_t nmemb, size_t size) {
  dbg_assert(here && here->gas && here->gas->local.calloc);
  return here->gas->local.calloc(nmemb, size);
}

inline static void *local_realloc(void *ptr, size_t size) {
  dbg_assert(here && here->gas && here->gas->local.realloc);
  return here->gas->local.realloc(ptr, size);
}

inline static void *local_valloc(size_t size) {
  dbg_assert(here && here->gas && here->gas->local.valloc);
  return here->gas->local.valloc(size);
}

inline static void *local_memalign(size_t boundary, size_t size) {
  dbg_assert(here && here->gas && here->gas->local.memalign);
  return here->gas->local.memalign(boundary, size);
}

inline static int local_posix_memalign(void **memptr, size_t alignment,
                                        size_t size) {
  dbg_assert(here && here->gas && here->gas->local.posix_memalign);
  return here->gas->local.posix_memalign(memptr, alignment, size);
}

inline static hpx_addr_t lva_to_gva(void *lva) {
  dbg_assert(here && here->gas && here->gas->lva_to_gva);
  return here->gas->lva_to_gva(lva);
}

inline static void *gva_to_lva(hpx_addr_t gva) {
  dbg_assert(here && here->gas && here->gas->gva_to_lva);
  return here->gas->gva_to_lva(gva);
}

#endif // LIBHPX_LOCALITY_H
