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

#include "hpx/types.h"
#include "hpx/runtime.h"

// ADK: How do we effectively capture data distribution hints? A
// straightforward way is to maintain a table mapping localities to
// their local allocation size, and have functions to fill this table
// up for common distributions such as cyclic, block-cyclic etc. 
typedef struct hpx_distribution_t {
  hpx_map_t dmap;
} hpx_distribution_t;

typedef struct hpx_addr_t {  
  hpx_locality_t locality;
} hpx_addr_t;

/*
 --------------------------------------------------------------------
 Address Resolution
 --------------------------------------------------------------------
*/
int hpx_addr_is_local(hpx_addr_t);

/*
 --------------------------------------------------------------------
 Memory Management
 --------------------------------------------------------------------
*/
hpx_addr_t hpx_malloc(hpx_distribution_t, size_t);

/*
 --------------------------------------------------------------------
 Data Distribution Functions
 --------------------------------------------------------------------
*/
/* We could have predefined distributions, but the number of
 * localities is variable, and a runtime parameter */
int cyclic_distribution(uint32, hpx_distribution_t *);
int block_cyclic_distribution(uint32, hpx_distribution_t *);
