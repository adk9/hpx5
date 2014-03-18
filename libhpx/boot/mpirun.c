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
}


boot_t *boot_new_mpirun(void) {
  int init;
  MPI_Initialized(&init);
  if (!init) {
    dbg_log("initializing MPI... ");
    int provided;
    if (MPI_Init_thread(0, NULL, MPI_THREAD_MULTIPLE, &provided)) {
      dbg_log("not available.\n");
      return NULL;
    }
    if (provided < MPI_THREAD_SERIALIZED) {
      dbg_log("not compatible.\n");
      return NULL;
    }
  }

  mpi_t *mpi = malloc(sizeof(*mpi));
  mpi->vtable->delete  = _delete;
  mpi->vtable->rank    = _rank;
  mpi->vtable->n_ranks = _n_ranks;

  if ((MPI_Comm_rank(MPI_COMM_WORLD, &mpi->rank) != MPI_SUCCESS) ||
      (MPI_Comm_size(MPI_COMM_WORLD, &mpi->n_ranks) != MPI_SUCCESS)) {
    free(mpi);
    return NULL;
  }

  return mpi;
}
