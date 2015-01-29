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
#include <mpi.h>
#include "libhpx/boot.h"
#include "libhpx/debug.h"


static HPX_RETURNS_NON_NULL const char *_id(void) {
  return "MPI";
}


static void _delete(boot_class_t *boot) {
  int finalized;
  MPI_Finalized(&finalized);
  if (!finalized)
    MPI_Finalize();
}


static int _rank(const boot_class_t *boot) {
  int rank;
  if ((MPI_Comm_rank(MPI_COMM_WORLD, &rank) != MPI_SUCCESS))
    hpx_abort();
  return rank;


}


static int _n_ranks(const boot_class_t *boot) {
  int ranks;
  if (MPI_Comm_size(MPI_COMM_WORLD, &ranks) != MPI_SUCCESS)
    hpx_abort();
  return ranks;
}


static int _barrier(const boot_class_t *boot) {
  if (MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS)
    return HPX_ERROR;
  return HPX_SUCCESS;
}


static int _allgather(const boot_class_t *boot, /* const */ void *in, void *out, int n) {
  int e = MPI_Allgather((void*)in, n, MPI_BYTE, out, n, MPI_BYTE, MPI_COMM_WORLD);
  if (e != MPI_SUCCESS)
    return dbg_error("mpirun: failed MPI_Allgather %d.\n", e);
  return HPX_SUCCESS;
}


static void _abort(const boot_class_t *boot) {
  MPI_Abort(MPI_COMM_WORLD, -6);
}


static boot_class_t _mpi = {
  .type      = HPX_BOOT_MPI,
  .id        = _id,
  .delete    = _delete,
  .rank      = _rank,
  .n_ranks   = _n_ranks,
  .barrier   = _barrier,
  .allgather = _allgather,
  .abort     = _abort
};


boot_class_t *boot_new_mpi(void) {
  int init;
  MPI_Initialized(&init);
  if (init)
    return &_mpi;

  if (MPI_Init(0, NULL) == MPI_SUCCESS) {
    log_boot("initialized MPI bootstrapper.\n");
    return &_mpi;
  }

  dbg_error("initialization failed.\n");
  return NULL;
}
