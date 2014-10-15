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

/// @file include/libhpx/locality.h
/// @brief Exports all of the resources available at an HPX locality.
///
/// The locality is modeled as a structure containing the basic ingredients of
/// the LIBHPX runtime present in each PE. It is exposed within the library
/// through the global "here" object that gets initialized during hpx_init().
///
/// In addition to this object, a small set of actions is available for inter-PE
/// AM-based communication.
///
/// Furthermore, this file defines a number of inline convenience functions that
/// wrap common functionality that needs access to the global here object.

#include "hpx/hpx.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"

/// Forward declarations.
/// @{
struct boot_class;
struct network_class;
struct scheduler;
struct transport_class;
struct hpx_config;
/// @}

/// The locality object.
///
/// @field      rank The dense, 0-based rank of this locality.
/// @field     ranks The total number of ranks running the current job.
/// @field      boot The bootstrap object. This provides rank and ranks, as well
///                  as some basic, IP-based networking functionality.
/// @field       gas The global address space object. This provides global
///                  memory allocation and address space functionality.
/// @field transport The byte transport object. This provides a basic,
///                  high-speed byte transport interface.
/// @field   network The parcel transport layer. This provides an active message
///                  interface targeting global addresses.
/// @field     sched The lightweight thread scheduler. This provides the
///                  infrastructure required to create lightweight threads, and
///                  to deal with inter-thread data and control dependencies
///                  using LCOs.
typedef struct {
  uint32_t                     rank;
  uint32_t                    ranks;
  struct boot_class           *boot;
  struct gas_class             *gas;
  struct transport_class *transport;
  struct network_class     *network;
  struct scheduler           *sched;
} locality_t;


/// Inter-locality action interface.
/// @{

/// Used to cause a locality to shutdown.
HPX_INTERNAL extern hpx_action_t locality_shutdown;

typedef struct {
  hpx_action_t action;
  hpx_status_t status;
  char data[];
} locality_cont_args_t;

HPX_INTERNAL extern hpx_action_t locality_call_continuation;
/// @}

/// The global locality is exposed through this "here" pointer.
HPX_INTERNAL extern locality_t *here;

/// A set of inline convenience functions.
/// @{

/// Check to see if a local virtual address is aliasing the global address
/// space.
inline static bool lva_is_global(void *addr) {
  dbg_assert(here && here->gas && here->gas->is_global);
  return here->gas->is_global(here->gas, addr);
}


/// Allocate global memory using the malloc interface and the global address
/// space implementation.
inline static void *global_malloc(size_t bytes) {
  dbg_assert(here && here->gas && here->gas->global.malloc);
  return here->gas->global.malloc(bytes);
}


/// Free global memory using the global address space implementation.
inline static void global_free(void *ptr) {
  dbg_assert(here && here->gas && here->gas->global.free);
  here->gas->global.free(ptr);
}


/// Allocate global memory using the calloc interface and the global address
/// space implementation.
inline static void *global_calloc(size_t nmemb, size_t size) {
  dbg_assert(here && here->gas && here->gas->global.calloc);
  return here->gas->global.calloc(nmemb, size);
}


/// Allocate global memory using the realloc interface and the global address
/// space implementation.
inline static void *global_realloc(void *ptr, size_t size) {
  dbg_assert(here && here->gas && here->gas->global.realloc);
  return here->gas->global.realloc(ptr, size);
}


/// Allocate global memory using the valloc interface and the global address
/// space implementation.
inline static void *global_valloc(size_t size) {
  dbg_assert(here && here->gas && here->gas->global.valloc);
  return here->gas->global.valloc(size);
}


/// Allocate global memory using the memalign interface and the global address
/// space implementation.
inline static void *global_memalign(size_t boundary, size_t size) {
  dbg_assert(here && here->gas && here->gas->global.memalign);
  return here->gas->global.memalign(boundary, size);
}


/// Allocate global memory using the posix memalign interface and the global
/// address space implementation.
inline static int global_posix_memalign(void **memptr, size_t alignment,
                                        size_t size) {
  dbg_assert(here && here->gas && here->gas->global.posix_memalign);
  return here->gas->global.posix_memalign(memptr, alignment, size);
}


/// Allocate local memory using the malloc interface and the global address
/// space implementation.
inline static void *local_malloc(size_t bytes) {
  dbg_assert(here && here->gas && here->gas->local.malloc);
  return here->gas->local.malloc(bytes);
}


/// Fee local memory using the global address space implementation.
inline static void local_free(void *ptr) {
  dbg_assert(here && here->gas && here->gas->local.free);
  here->gas->local.free(ptr);
}


/// Allocate local memory using the calloc interface and the global address
/// space implementation.
inline static void *local_calloc(size_t nmemb, size_t size) {
  dbg_assert(here && here->gas && here->gas->local.calloc);
  return here->gas->local.calloc(nmemb, size);
}

/// Allocate local memory using the realloc interface and the global address
/// space implementation.
inline static void *local_realloc(void *ptr, size_t size) {
  dbg_assert(here && here->gas && here->gas->local.realloc);
  return here->gas->local.realloc(ptr, size);
}


/// Allocate local memory using the valloc interface and the global address
/// space implementation.
inline static void *local_valloc(size_t size) {
  dbg_assert(here && here->gas && here->gas->local.valloc);
  return here->gas->local.valloc(size);
}


/// Allocate local memory using the memalign interface and the global address
/// space implementation.
inline static void *local_memalign(size_t boundary, size_t size) {
  dbg_assert(here && here->gas && here->gas->local.memalign);
  return here->gas->local.memalign(boundary, size);
}


/// Allocate local memory using the posix memalign interface and the global
/// address space implementation.
inline static int local_posix_memalign(void **memptr, size_t alignment,
                                        size_t size) {
  dbg_assert(here && here->gas && here->gas->local.posix_memalign);
  return here->gas->local.posix_memalign(memptr, alignment, size);
}


/// Convert a local virtual address to a global virtual address using the global
/// address space.
inline static hpx_addr_t lva_to_gva(void *lva) {
  dbg_assert(here && here->gas && here->gas->lva_to_gva);
  return here->gas->lva_to_gva(lva);
}


/// Convert a global virtual address to a local virtual address using the global
/// address space.
inline static void *gva_to_lva(hpx_addr_t gva) {
  dbg_assert(here && here->gas && here->gas->gva_to_lva);
  return here->gas->gva_to_lva(gva);
}
/// @}

#endif // LIBHPX_LOCALITY_H
