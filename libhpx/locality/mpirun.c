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
#include "manager.h"

static void _delete(manager_t *pmi) {
  int finalized;
  MPI_Finalized(&finalized);
  if (!finalized)
    MPI_Finalize();
  free(pmi);
}

manager_t *
manager_new_mpirun(void) {
  int init;
  MPI_Initialized(init);
  if (!init) {
    int provided;
    if (MPI_Init_thread(0, NULL, MPI_THREAD_MULTIPLE, &provided))
      return NULL;
    if (provided < MPI_THREAD_SERIALIZED)
      return NULL;
  }

  int n_ranks;
  if (MPI_Comm_size(MPI_COMM_WORLD, &n_ranks) != MPI_SUCCESS)
    return NULL;

  int rank;
  if (MPI_Comm_rank(MPI_COMM_WORLD, &rank) != MPI_SUCCESS)
    return NULL;

  manager_t *mpi = malloc(sizeof(*mpi));
  mpi->delete    = _delete;
  mpi->rank      = rank;
  mpi->n_ranks   = n_ranks;
  return mpi;
}
