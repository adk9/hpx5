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

static const char * const _smp_id_string = "SMP";

static const char *_smp_id(void) {
  return _smp_id_string;
}


static void _delete(boot_class_t *boot) {
  free(boot);
}


static int _rank(const boot_class_t *boot) {
  return 0;
}


static int _n_ranks(const boot_class_t *boot) {
  return 1;
}


static int _barrier(const boot_class_t *boot) {
  return 0;
}


static int _allgather(const boot_class_t *boot, /* const */ void *in, void *out, int n) {
  return 0;
}


static void _abort(const boot_class_t *boot) {
  abort();
}


boot_class_t *boot_new_smp(void) {
  boot_class_t *smp = malloc(sizeof(*smp));
  smp->type      = HPX_BOOT_SMP;
  smp->id        = _smp_id;
  smp->delete    = _delete;
  smp->rank      = _rank;
  smp->n_ranks   = _n_ranks;
  smp->barrier   = _barrier;
  smp->allgather = _allgather;
  smp->abort     = _abort;
  return smp;
}
