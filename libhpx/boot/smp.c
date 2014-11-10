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


static const char *_id(void) {
  return "SMP";
}


static void _delete(boot_class_t *boot) {
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


static boot_class_t _smp = {
  .type      = HPX_BOOT_SMP,
  .id        = _id,
  .delete    = _delete,
  .rank      = _rank,
  .n_ranks   = _n_ranks,
  .barrier   = _barrier,
  .allgather = _allgather,
  .abort     = _abort
};


boot_class_t *boot_new_smp(void) {
  return &_smp;
}
