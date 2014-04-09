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
#include "managers.h"


typedef struct {
  boot_t vtable;
  int rank;
  int n_ranks;
} mpi_t;


static void _delete(boot_t *boot) {
  int finalized;
  MPI_Finalized(&finalized);
  if (!finalized)
    MPI_Finalize();
  free(boot);
}


static int _rank(const boot_t *boot) {
  const mpi_t *mpi = (const mpi_t *)boot;
  return mpi->rank;
}


static int _n_ranks(const boot_t *boot) {
  const mpi_t *mpi = (const mpi_t*)boot;
  return mpi->n_ranks;
}


static int _barrier(void) {
  if (MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS)
    return HPX_ERROR;
  return HPX_SUCCESS;
}


static int _allgather(const boot_t *boot, const void *in, void *out, int n) {
  int e = MPI_Allgather((void*)in, n, MPI_BYTE, out, n, MPI_BYTE, MPI_COMM_WORLD);
  if (e != MPI_SUCCESS)
    return dbg_error("failed mpi->allgather().\n");
  return HPX_SUCCESS;
}


boot_t *boot_new_mpi(void) {
  int init;
  MPI_Initialized(&init);
  if (!init) {
    dbg_log("initializing MPI boostrap... ");
    if (MPI_Init(0, NULL) != MPI_SUCCESS) {
      dbg_error("failed to bootstrap with MPI.\n");
      return NULL;
    }
  }

  mpi_t *mpi = malloc(sizeof(*mpi));
  mpi->vtable.delete    = _delete;
  mpi->vtable.rank      = _rank;
  mpi->vtable.n_ranks   = _n_ranks;
  mpi->vtable.barrier   = _barrier;
  mpi->vtable.allgather = _allgather;

  if ((MPI_Comm_rank(MPI_COMM_WORLD, &mpi->rank) != MPI_SUCCESS) ||
      (MPI_Comm_size(MPI_COMM_WORLD, &mpi->n_ranks) != MPI_SUCCESS)) {
    free(mpi);
    return NULL;
  }

  return &mpi->vtable;
}
