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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <mpi.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>

static HPX_RETURNS_NON_NULL const char *_id(void) {
  return "MPI";
}

static void _delete(boot_t *boot) {
  int finalized;
  MPI_Finalized(&finalized);
  if (!finalized) {
    MPI_Finalize();
  }
}

static int _rank(const boot_t *boot) {
  int rank;
  if (MPI_Comm_rank(MPI_COMM_WORLD, &rank) != MPI_SUCCESS) {
    hpx_abort();
  }
  return rank;
}

static int _n_ranks(const boot_t *boot) {
  int ranks;
  if (MPI_Comm_size(MPI_COMM_WORLD, &ranks) != MPI_SUCCESS) {
    hpx_abort();
  }
  return ranks;
}

static int _barrier(const boot_t *boot) {
  if (MPI_Barrier(MPI_COMM_WORLD) != MPI_SUCCESS) {
    return HPX_ERROR;
  }
  return LIBHPX_OK;
}

static int _allgather(const boot_t *boot, const void *restrict src,
                      void *restrict dest, int n) {
  int e = MPI_Allgather((void *)src, n, MPI_BYTE, dest, n, MPI_BYTE, MPI_COMM_WORLD);
  if (MPI_SUCCESS != e) {
    dbg_error("failed MPI_Allgather %d.\n", e);
  }
  return LIBHPX_OK;
}

static int _mpi_alltoall(const void *boot, void *restrict dest,
                         const void *restrict src, int n, int stride) {
  const boot_t *mpi = boot;
  int ranks = mpi->n_ranks(mpi);
  int *counts = calloc(ranks, sizeof(*counts));
  int *offsets = calloc(ranks, sizeof(*offsets));
  assert(offsets);
  assert(counts);
  for (int i = 0, e = ranks; i < e; ++i) {
    counts[i] = n;
    offsets[i] = i * stride;
  }
  if (MPI_SUCCESS != MPI_Alltoallv((void *)src, counts, offsets, MPI_BYTE,
                                   dest, counts, offsets, MPI_BYTE,
                                   MPI_COMM_WORLD)) {
    dbg_error("MPI_Alltoallv failed at bootstrap\n");
  }
  free(offsets);
  free(counts);
  return LIBHPX_OK;
}

static void _abort(const boot_t *boot) {
  MPI_Abort(MPI_COMM_WORLD, -6);
}

static boot_t _mpi_boot_class = {
  .type      = HPX_BOOT_MPI,
  .id        = _id,
  .delete    = _delete,
  .rank      = _rank,
  .n_ranks   = _n_ranks,
  .barrier   = _barrier,
  .allgather = _allgather,
  .alltoall  = _mpi_alltoall,
  .abort     = _abort
};

boot_t *boot_new_mpi(void) {
  int init;
  if (MPI_SUCCESS != MPI_Initialized(&init)) {
    log_error("mpi initialization failed\n");
    return NULL;
  }

  if (init) {
    return &_mpi_boot_class;
  }

  static const int LIBHPX_THREAD_LEVEL = MPI_THREAD_FUNNELED;

  int level;
  if (MPI_SUCCESS != MPI_Init_thread(NULL, NULL, LIBHPX_THREAD_LEVEL, &level)) {
    log_error("mpi initialization failed\n");
    return NULL;
  }

  if (level != LIBHPX_THREAD_LEVEL) {
    log_boot("MPI thread level failed requested %d, received %d.\n",
         LIBHPX_THREAD_LEVEL, level);
  }

  log_boot("thread_support_provided = %d\n", level);
  return &_mpi_boot_class;
}
