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

#ifndef HPX_AGAS_H_
#define HPX_AGAS_H_

#include <stdbool.h>                            /* bool */
#include "hpx/runtime.h"                        /* hpx_locality_t */

/**
 * @brief A global virtual address.
 *
 * This is exposed as a value type to the application programmer. This has a
 * number of implications, primarily that it breaks a level of abstraction and
 * requires the application to be recompiled if the struct changes. This has
 * some portability issues, but doesn't require that the application manage
 * addresses using a create/destroy interface---an important concern when the
 * application has to do address arithmetic.
 */
struct hpx_addr {
  hpx_locality_t locality;
};

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
bool hpx_addr_get_local(struct hpx_addr addr, void **out);

#endif /* HPX_AGAS_H_ */
