/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_BOOTSTRAP_MPIRUN_H_
#define LIBHPX_BOOTSTRAP_MPIRUN_H_

#include <stdlib.h>

#include "bootstrap.h"
struct hpx_locality;
extern bootstrap_ops_t mpi_boot_ops;

/**
 * mpirun bootstrap operations
 */

int bootstrap_mpi_init(void);
int bootstrap_mpi_get_rank(void);
int bootstrap_mpi_get_addr(struct hpx_locality *);
int bootstrap_mpi_size(void);
int bootstrap_mpi_get_map(struct hpx_locality **);
int bootstrap_mpi_finalize(void);

#endif /* LIBHPX_BOOTSTRAP_MPIRUN_H_ */
