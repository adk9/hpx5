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
#ifndef LIBHPX_BOOT_MANAGERS_H
#define LIBHPX_BOOT_MANAGERS_H

struct boot {
  void (*delete)(boot_t*);
  int (*rank)(const boot_t*);
  int (*n_ranks)(const boot_t*);
  int (*barrier)(void);
  int (*allgather)(const boot_t*, const void*, void*, int);
};

HPX_INTERNAL boot_t *boot_new_mpi(void);
HPX_INTERNAL boot_t *boot_new_pmi(void);
HPX_INTERNAL boot_t *boot_new_smp(void);


#endif // LIBHPX_BOOT_MANAGERS_H
