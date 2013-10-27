/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  mpirun bootstrap component
  mpirun.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_MPI
#include <mpi.h>
#endif

#include "bootstrap/bootstrap.h"
#include "network.h"
#include "hpx/action.h"
#include "hpx/init.h"
#include "hpx/parcel.h"
#include "hpx/runtime.h"

#include "bootstrap_mpirun.h"

static int rank;
static int size;

/* MPI network operations */
bootstrap_ops_t mpi_boot_ops = {
  .init     = bootstrap_mpi_init,
  .get_rank = bootstrap_mpi_get_rank,
  .get_addr = bootstrap_mpi_get_addr,
  .get_map  = bootstrap_mpi_get_map,
  .size     = bootstrap_mpi_size,
  .finalize = bootstrap_mpi_finalize,
};

int bootstrap_mpi_init(void) {
  int ret;

  ret = HPX_ERROR;
  MPI_Initialized(&ret);
  if (!ret) {
    int provided;
    if (MPI_Init_thread(0, NULL, MPI_THREAD_SERIALIZED, &provided))
      goto err;
    if (provided != MPI_THREAD_SERIALIZED)
      goto err;
  }

  if (MPI_Comm_size(MPI_COMM_WORLD, &size) != MPI_SUCCESS)
    goto err;

  if (MPI_Comm_rank(MPI_COMM_WORLD, &rank) != MPI_SUCCESS)
    goto err;

  return 0;

err:    
  return HPX_ERROR;
}

int bootstrap_mpi_get_rank(void) {
  return rank;
}

int bootstrap_mpi_get_addr(hpx_locality_t *l) {
  return __hpx_network_ops->phys_addr(l);
}

int bootstrap_mpi_size(void) {
  return size;
}

int bootstrap_mpi_get_map(hpx_locality_t **map) {
  int ret;
  hpx_locality_t *loc;

  *map = NULL;
  loc = hpx_get_my_locality();
  if (!loc) return HPX_ERROR;

  *map = hpx_alloc(size * sizeof(hpx_locality_t));
  if (*map == NULL) return HPX_ERROR_NOMEM;

  ret = MPI_Allgather(loc, sizeof(*loc), MPI_BYTE, *map, sizeof(*loc), MPI_BYTE, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS) {
    free(*map);
    *map = NULL;
    return HPX_ERROR;
  }
  return 0;
}

int bootstrap_mpi_finalize(void) {
  int ret;

  MPI_Finalized(&ret);
  if (!ret)
    MPI_Finalize();
  return 0;
}
