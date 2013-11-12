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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>

#if HAVE_MPI
#include <mpi.h>
#endif

#include "bootstrap.h"
#include "hpx/error.h"
#include "hpx/globals.h"                        /* __hpx_network_ops */
#include "hpx/mem.h"                            /* hpx_{alloc,free} */
#include "hpx/types.h"                          /* hpx_locality_t */
#include "network.h"                            /* typedef hpx_network_ops */

static int _rank;
static int _size;

static int init(void);
static int get_rank(void);
static int get_addr(struct hpx_locality *);
static int size(void);
static int get_map(struct hpx_locality **);
static int finalize(void);

/* MPI network operations */
bootstrap_ops_t mpi_boot_ops = {
  .init     = init,
  .get_rank = get_rank,
  .get_addr = get_addr,
  .get_map  = get_map,
  .size     = size,
  .finalize = finalize,
};

int init(void) {
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

  if (MPI_Comm_size(MPI_COMM_WORLD, &_size) != MPI_SUCCESS)
    goto err;

  if (MPI_Comm_rank(MPI_COMM_WORLD, &_rank) != MPI_SUCCESS)
    goto err;

  return 0;

err:    
  return HPX_ERROR;
}

int get_rank(void) {
  return _rank;
}

int get_addr(hpx_locality_t *l) {
  return __hpx_network_ops->phys_addr(l);
}

int size(void) {
  return _size;
}

int get_map(hpx_locality_t **map) {
  int ret;
  hpx_locality_t *loc;

  *map = NULL;
  loc = hpx_get_my_locality();
  if (!loc) return HPX_ERROR;

  *map = hpx_alloc(size() * sizeof(hpx_locality_t));
  if (*map == NULL) return HPX_ERROR_NOMEM;

  ret = MPI_Allgather(loc, sizeof(*loc), MPI_BYTE, *map, sizeof(*loc), MPI_BYTE, MPI_COMM_WORLD);
  if (ret != MPI_SUCCESS) {
    hpx_free(*map);
    *map = NULL;
    return HPX_ERROR;
  }
  return 0;
}

int finalize(void) {
  int ret;

  MPI_Finalized(&ret);
  if (!ret)
    MPI_Finalize();
  return 0;
}
