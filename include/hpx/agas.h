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
#ifndef HPX_AGAS_H_
#define HPX_AGAS_H_

#include <stdbool.h>
#include "runtime.h"                            /* hpx_locality_t */

// ADK: How do we effectively capture data distribution hints? A
// straightforward way is to maintain a table mapping localities to
// their local allocation size, and have functions to fill this table
// up for common distributions such as cyclic, block-cyclic etc. 

/**
 * A data distribution.
 */
typedef struct hpx_distribution hpx_distribution_t;

/**
 * A global address.
 */
typedef struct hpx_addr {
  hpx_locality_t locality;
} hpx_addr_t;

/**
 * Get a local address for a global address.
 *
 * @param[in]  addr - the global address to query
 * @param[out] out  - the local address, or NULL if the address is remote
 *
 * @eturns true if the address was local, false otherwise---this allows us to
 *         disambiguate between the local NULL address and a remote address
 *         during mapping
 */
bool hpx_addr_get_local(hpx_addr_t addr, void **out);

/**
 * Allocate a global array.
 *
 * @param[in] dist  - the distribution for the array
 * @param[in] count - the number of elements to allocate
 * @param[in] bytes - the size of each element, in bytes
 *
 * @returns the base global address for the array
 */
hpx_addr_t hpx_malloc(hpx_distribution_t dist, size_t count, size_t bytes);

/** Create distributions. @{ */
int cyclic_distribution(uint32, hpx_distribution_t *);
int block_cyclic_distribution(uint32, hpx_distribution_t *);
/** @} */

#endif /* HPX_AGAS_H_ */
