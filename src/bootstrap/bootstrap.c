/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Networkunication Layer
  network.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <limits.h>
#include <stdlib.h>

#include "bootstrap.h"
#include "network.h"
#include "hpx/parcel.h"
#include "hpx/runtime.h" /* for hpx_locality_t */

/* Default bootstrap operations */
bootstrap_ops_t default_boot_ops = {
    .init     = hpx_bootstrap_init,
    .get_rank = hpx_bootstrap_get_rank,
    .get_addr = hpx_bootstrap_get_addr,
    .get_map  = hpx_bootstrap_get_map,
    .size     = hpx_bootstrap_size,
    .finalize = hpx_bootstrap_finalize,
};

/*
 * Stub versions
 */

int hpx_bootstrap_init(void) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int hpx_bootstrap_get_rank(void) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR; 
}

int hpx_bootstrap_get_addr(hpx_locality_t *l) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR; 
}

int hpx_bootstrap_size(void) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int hpx_bootstrap_get_map(hpx_locality_t **map) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}

int hpx_bootstrap_finalize(void) {
  __hpx_errno = HPX_ERROR;
  return HPX_ERROR;
}
