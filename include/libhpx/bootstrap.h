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
#ifndef LIBHPX_BOOTSTRAP_H_
#define LIBHPX_BOOTSTRAP_H_

/**
 * @file
 * @brief Provides the interface to the bootstrap class.
 */

#include "hpx/runtime.h"

/**
 * The bootstrap_ops interface.
 *
 * Declares the abstract interface tot he bootstrap_ops class.
 */
typedef struct bootstrap_ops {
  /* Initialize the bootstrap module */
  int (*init)(void);
  /* Get identifier/rank of the calling locality */
  int (*get_rank)(void);
  /* Get the physical network address of current locality */
  int (*get_addr)(struct hpx_locality *);
  /* Get the total number of participating ranks */
  int (*size)(void);
  /* Get the logical (rank -> addr) map of the bootstrapped network */
  int (*get_map)(struct hpx_locality **);
  /* Shutdown and clean up the bootstrap module */
  int (*finalize)(void);
} bootstrap_ops_t;

/**
 * Declare the concrete bootstrap_ops classes that we have available.
 * @{
 */
extern bootstrap_ops_t default_boot_ops;
extern bootstrap_ops_t mpi_boot_ops;
/**
 * @}
 */

/**
 * Bootstrap components.
 */
typedef struct bootstrap_comp {
  char             name[128];
  bootstrap_ops_t *ops;
  int             *flags;
  int              active;
} bootstrap_comp_t;

typedef struct bootstrap_mgr {
  /* Default bootstrapping mechanism. */
  bootstrap_comp_t *defaults;
  int count;
  /* List of registered bootstrap components.  */
  bootstrap_comp_t *trans;
} bootstrap_mgr_t;

#endif /* LIBHPX_BOOTSTRAP_H_ */
