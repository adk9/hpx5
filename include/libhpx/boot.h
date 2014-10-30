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
#ifndef LIBHPX_BOOT_H
#define LIBHPX_BOOT_H

#include "hpx/attributes.h"
#include "hpx/hpx.h"
#include "libhpx/config.h"

typedef struct boot_class boot_class_t;
struct boot_class {
  hpx_boot_t type;
  void (*delete)(boot_class_t*);
  int (*rank)(const boot_class_t*);
  int (*n_ranks)(const boot_class_t*);
  int (*barrier)(const boot_class_t*);
  int (*allgather)(const boot_class_t*, /* const */ void*, void*, int);
  void (*abort)(const boot_class_t*);
};



HPX_INTERNAL boot_class_t *boot_new_mpi(void);
HPX_INTERNAL boot_class_t *boot_new_pmi(void);
HPX_INTERNAL boot_class_t *boot_new_smp(void);
HPX_INTERNAL boot_class_t *boot_new(hpx_boot_t type);


static inline void boot_delete(boot_class_t *boot) {
  boot->delete(boot);
}

static inline hpx_boot_t boot_type(boot_class_t *boot) {
  return boot->type;
}

static inline int boot_rank(const boot_class_t *boot) {
  return boot->rank(boot);
}


static inline int boot_n_ranks(const boot_class_t *boot) {
  return boot->n_ranks(boot);
}


static inline int boot_allgather(const boot_class_t *boot, /* const */ void *in,
                                 void *out, int n) {
  return boot->allgather(boot, in, out, n);
}


static inline int boot_barrier(const boot_class_t *boot) {
  return boot->barrier(boot);
}


static inline void boot_abort(const boot_class_t *boot) {
  boot->abort(boot);
}

#endif // LIBHPX_BOOT_H
