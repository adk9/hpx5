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

#ifndef HPX_MALLOC_H_
#define HPX_MALLOC_H_

#include <stddef.h>                             /* size_t */
#include <stdint.h>                             /* uint32_t */
#include "hpx/agas.h"                           /* hpx_addr_t */

typedef struct hpx_distribution hpx_distribution_t;

/** Create distributions. @{ */
int cyclic_distribution(uint32_t, hpx_distribution_t *);
int block_cyclic_distribution(uint32_t, hpx_distribution_t *);
/** @} */

/**
 * Allocate a global array.
 *
 * @param[in] dist  - the distribution for the array
 * @param[in] count - the number of elements to allocate
 * @param[in] bytes - the size of each element, in bytes
 *
 * @returns the base global address for the array
 */
hpx_addr_t hpx_malloc(hpx_distribution_t *dist, size_t count, size_t bytes);

#endif /* HPX_MALLOC_H_ */
