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

#include <stdlib.h>

#include "libhpx/boot.h"
#include "managers.h"

typedef struct {
  boot_t vtable;
  int rank;
  int n_ranks;
} smp_t;

static void _delete(boot_t *boot) {
  free(boot);
}

static int _rank(const boot_t *boot) {
  const smp_t *smp = (const smp_t*)boot;
  return smp->rank;
}

static int _n_ranks(const boot_t *boot) {
  const smp_t *smp = (const smp_t*)boot;
  return smp->n_ranks;
}

boot_t *boot_new_smp(void) {
  smp_t *smp = malloc(sizeof(*smp));
  smp->vtable.delete  = _delete;
  smp->vtable.rank    = _rank;
  smp->vtable.n_ranks = _n_ranks;
  smp->rank           = 0;
  smp->n_ranks        = 1;
  return &smp->vtable;
}
