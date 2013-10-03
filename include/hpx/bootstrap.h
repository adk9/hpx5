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

#include <stdlib.h>

#include "hpx/network.h"

typedef struct bootstrap_ops_t {
  /* Initialize the bootstrap module */
  int (*init)(void);
  /* Get a unique identifier/rank */
  void (*id)(void *data);
  /* Get the total number of participating ranks */
  int (*size)(void);
  /* Get the logical map of the bootstrapped network */
  int (*get_map)(void);
  /* Shutdown and clean up the bootstrap module */
  int (*finalize)(void);
} bootstrap_ops_t;

extern bootstrap_ops_t default_boot_ops;

/**
 * Bootstrap components.
 */
typedef struct bootstrap_comp_t {
  char             name[128];
  bootstrap_ops_t *ops;
  int             *flags;
  int              active;
} bootstrap_comp_t;

typedef struct bootstrap_mgr_t {
  /* Default bootstrapping mechanism. */
  bootstrap_comp_t *default;
  int count;
  /* List of registered bootstrap components.  */
  bootstrap_comp_t *trans;
} bootstrap_mgr_t;

/**
 * Default bootstrap operations
 */

/* Initialize the bootstrap layer */
int hpx_bootstrap_init(void);

/* Get a unique identifier/rank */
void hpx_bootstrap_id(void *data);

/* Get the total number of participating ranks */
int hpx_bootstrap_size(void);

/* Get the logical map of the bootstrapped network */
int hpx_bootstrap_get_map(void);

/* Shutdown and clean up the bootstrap layer */
int hpx_bootstrap_finalize(void);


#endif /* LIBHPX_BOOTSTRAP_H_ */
