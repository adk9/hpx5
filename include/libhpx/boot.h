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
#ifndef LIBHPX_BOOT_H
#define LIBHPX_BOOT_H

#include "hpx/attributes.h"
#include "hpx/hpx.h"
#include "libhpx/config.h"

typedef struct boot {
  hpx_boot_t type;

  const char *(*id)(void);

  void (*delete)(struct boot*);
  int (*rank)(const struct boot*);
  int (*n_ranks)(const struct boot*);
  int (*barrier)(const struct boot*);
  int (*allgather)(const struct boot*, const void*, void*, int);
  void (*abort)(const struct boot*);
} boot_t;

boot_t *boot_new_mpi(void) HPX_INTERNAL;
boot_t *boot_new_pmi(void) HPX_INTERNAL;
boot_t *boot_new_smp(void) HPX_INTERNAL;
boot_t *boot_new(hpx_boot_t type) HPX_INTERNAL;

static inline void boot_delete(boot_t *boot) {
  boot->delete(boot);
}

static inline hpx_boot_t boot_type(boot_t *boot) {
  return boot->type;
}

static inline int boot_rank(const boot_t *boot) {
  return boot->rank(boot);
}

static inline int boot_n_ranks(const boot_t *boot) {
  return boot->n_ranks(boot);
}

static inline int boot_allgather(const boot_t *boot,
                                 const void *in, void *out, int n) {
  return boot->allgather(boot, in, out, n);
}

static inline int boot_barrier(const boot_t *boot) {
  return boot->barrier(boot);
}

static inline void boot_abort(const boot_t *boot) {
  boot->abort(boot);
}

#endif
