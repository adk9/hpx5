// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include <stdbool.h>
#include <stdlib.h>
#include <mpi.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>

static const int LIBHPX_THREAD_LEVEL = MPI_THREAD_SERIALIZED;

typedef struct {
  boot_t vtable;
  MPI_Comm comm;
  int      fini;
} _mpi_boot_t;

static const char *
_id(void)
{
  return "MPI";
}

static void
_delete(boot_t *boot)
{
  _mpi_boot_t *mpi = (_mpi_boot_t *)boot;
  if (mpi->fini) {
    MPI_Finalize();
  }
  free(mpi);
}

static int
_rank(const boot_t *boot)
{
  int rank;
  const _mpi_boot_t *mpi = (const _mpi_boot_t *)boot;
  if (MPI_Comm_rank(mpi->comm, &rank) != MPI_SUCCESS) {
    hpx_abort();
  }
  return rank;
}

static int
_n_ranks(const boot_t *boot)
{
  int ranks;
  const _mpi_boot_t *mpi = (const _mpi_boot_t *)boot;
  if (MPI_Comm_size(mpi->comm, &ranks) != MPI_SUCCESS) {
    hpx_abort();
  }
  return ranks;
}

static int
_barrier(const boot_t *boot)
{
  const _mpi_boot_t *mpi = (const _mpi_boot_t *)boot;
  return (MPI_Barrier(mpi->comm) != MPI_SUCCESS) ? HPX_ERROR : LIBHPX_OK;
}

static int
_allgather(const boot_t *boot, const void *restrict src, void *restrict dest,
           int n)
{
  const _mpi_boot_t *mpi = (const _mpi_boot_t *)boot;
  int e = MPI_Allgather((void *)src, n, MPI_BYTE, dest, n, MPI_BYTE, mpi->comm);
  if (MPI_SUCCESS != e) {
    dbg_error("failed MPI_Allgather %d.\n", e);
  }
  return LIBHPX_OK;
}

static int
_alltoall(const void *boot, void *restrict dest, const void *restrict src,
          int n, int stride)
{
  const _mpi_boot_t *mpi = boot;
  int ranks = _n_ranks(boot);
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
                                   mpi->comm)) {
    dbg_error("MPI_Alltoallv failed at bootstrap\n");
  }
  free(offsets);
  free(counts);
  return LIBHPX_OK;
}

static void
_abort(const boot_t *boot)
{
  const _mpi_boot_t *mpi = (const _mpi_boot_t *)boot;
  MPI_Abort(mpi->comm, -6);
}

boot_t *
boot_new_mpi(void)
{
  _mpi_boot_t *mpi = malloc(sizeof(*mpi));
  mpi->vtable.id        = _id;
  mpi->vtable.delete    = _delete;
  mpi->vtable.rank      = _rank;
  mpi->vtable.n_ranks   = _n_ranks;
  mpi->vtable.barrier   = _barrier;
  mpi->vtable.allgather = _allgather;
  mpi->vtable.alltoall  = _alltoall;
  mpi->vtable.abort     = _abort;

  int already_initialized = 0;
  if (MPI_SUCCESS != MPI_Initialized(&already_initialized)) {
    log_error("mpi initialization failed\n");
    free(mpi);
    return NULL;
  }

  if (!already_initialized) {
    int level = MPI_THREAD_SINGLE;
    if (MPI_SUCCESS != MPI_Init_thread(NULL, NULL, LIBHPX_THREAD_LEVEL, &level)) {
      log_error("mpi initialization failed\n");
      free(mpi);
      return NULL;
    }

    if (level != LIBHPX_THREAD_LEVEL) {
      log_error("MPI thread level failed requested %d, received %d.\n",
                LIBHPX_THREAD_LEVEL, level);
      free(mpi);
      return NULL;
    }
  }

  // remember if we need to finalize MPI and use a duplication COMM_WORLD
  mpi->fini = !already_initialized;
  if (MPI_SUCCESS != MPI_Comm_dup(MPI_COMM_WORLD, &mpi->comm)) {
    log_error("mpi communicator duplication failed\n");
    free(mpi);
    return NULL;
  }

  log_boot("thread_support_provided = %d\n", LIBHPX_THREAD_LEVEL);
  return &mpi->vtable;
}
