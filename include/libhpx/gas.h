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
#ifndef LIBHPX_GAS_H
#define LIBHPX_GAS_H

#include <hpx/hpx.h>

/// Generic object oriented interface to the global address space.
typedef struct gas_class gas_class_t;
struct gas_class {
  hpx_gas_t type;

  void (*delete)(gas_class_t *gas);
  int (*join)(gas_class_t *gas);
  void (*leave)(gas_class_t *gas);

  void *(*malloc)(gas_class_t *gas, size_t bytes);
  void  (*free)(gas_class_t *gas, void *ptr);
  void *(*calloc)(gas_class_t *gas, size_t nmemb, size_t size);
  void *(*realloc)(gas_class_t *gas, void *ptr, size_t size);
  void *(*valloc)(gas_class_t *gas, size_t size);
  void *(*memalign)(gas_class_t *gas, size_t boundary, size_t size);
  int   (*posix_memalign)(gas_class_t *gas, void **memptr, size_t alignment, size_t size);

  void *(*local_malloc)(gas_class_t *gas, size_t bytes);
  void  (*local_free)(gas_class_t *gas, void *ptr);
  void *(*local_calloc)(gas_class_t *gas, size_t nmemb, size_t size);
  void *(*local_realloc)(gas_class_t *gas, void *ptr, size_t size);
  void *(*local_valloc)(gas_class_t *gas, size_t size);
  void *(*local_memalign)(gas_class_t *gas, size_t boundary, size_t size);
  int   (*local_posix_memalign)(gas_class_t *gas, void **memptr, size_t alignment, size_t size);
};

gas_class_t *gas_local_only_new(size_t heap_size) HPX_INTERNAL;
gas_class_t *gas_pgas_new(size_t heap_size) HPX_INTERNAL;
gas_class_t *gas_agas_new(size_t heap_size) HPX_INTERNAL;
gas_class_t *gas_agas_switch_new(size_t heap_size) HPX_INTERNAL;
gas_class_t *gas_new(hpx_gas_t type, size_t heap_size) HPX_INTERNAL;

inline static void gas_delete(gas_class_t *gas) {
  gas->delete(gas);
}

inline static int gas_join(gas_class_t *gas) {
  return gas->join(gas);
}

inline static void gas_leave(gas_class_t *gas) {
  gas->leave(gas);
}

inline static void *gas_malloc(gas_class_t *gas, size_t bytes) {
  return gas->malloc(gas, bytes);
}

inline static void gas_free(gas_class_t *gas, void *ptr) {
  gas->free(gas, ptr);
}

inline static void *gas_calloc(gas_class_t *gas, size_t nmemb, size_t size) {
  return gas->calloc(gas, nmemb, size);
}

inline static void *gas_realloc(gas_class_t *gas, void *ptr, size_t size) {
  return gas->realloc(gas, ptr, size);
}

inline static void *gas_valloc(gas_class_t *gas, size_t size) {
  return gas->valloc(gas, size);
}

inline static void *gas_memalign(gas_class_t *gas, size_t boundary,
                                 size_t size) {
  return gas->memalign(gas, boundary, size);
}

inline static int gas_posix_memalign(gas_class_t *gas, void **memptr,
                                     size_t alignment, size_t size) {
  return gas->posix_memalign(gas, memptr, alignment, size);
}

inline static void *gas_local_malloc(gas_class_t *gas, size_t bytes) {
  return gas->local_malloc(gas, bytes);
}

inline static void gas_local_free(gas_class_t *gas, void *ptr) {
  gas->local_free(gas, ptr);
}

inline static void *gas_local_calloc(gas_class_t *gas, size_t nmemb,
                                     size_t size) {
  return gas->local_calloc(gas, nmemb, size);
}

inline static void *gas_local_realloc(gas_class_t *gas, void *ptr,
                                      size_t size) {
  return gas->local_realloc(gas, ptr, size);
}

inline static void *gas_local_valloc(gas_class_t *gas, size_t size) {
  return gas->local_valloc(gas, size);
}

inline static void *gas_local_memalign(gas_class_t *gas, size_t boundary,
                                       size_t size) {
  return gas->local_memalign(gas, boundary, size);
}

inline static int gas_local_posix_memalign(gas_class_t *gas, void **memptr,
                                           size_t alignment, size_t size) {
  return gas->local_posix_memalign(gas, memptr, alignment, size);
}

#endif// LIBHPX_GAS_H
