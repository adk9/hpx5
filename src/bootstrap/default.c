/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Default boostrap implementation
  default.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hpx/error.h"                          /* __hpx_errno, HPX_ERROR */
#include "bootstrap.h"                          /* default_boot_ops */

static int init(void);
static int get_rank(void);
static int get_addr(hpx_locality_t *);
static int size(void);
static int get_map(hpx_locality_t **);
static int finalize(void);

/* Default bootstrap operations */
bootstrap_ops_t default_boot_ops = {
    .init     = init,
    .get_rank = get_rank,
    .get_addr = get_addr,
    .get_map  = get_map,
    .size     = size,
    .finalize = finalize,
};

int
init(void) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int
finalize(void) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int
get_rank(void) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR; 
}

int
get_addr(hpx_locality_t *l) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR; 
}

int
size(void) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int
get_map(hpx_locality_t **map) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}
