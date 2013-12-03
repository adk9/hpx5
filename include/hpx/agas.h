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

#include <stdbool.h>

/** Structure types provided by this header. @{ */
typedef struct hpx_addr hpx_addr_t;
/** @} */

/**
 * A global virtual address.
 *
 * This is exposed as a value type to the application programmer. This has a
 * number of implications, primarily that it breaks a level of abstraction and
 * requires the application to be recompiled if the struct changes. This has
 * some portability issues, but doesn't require that the application manage
 * addresses using a create/destroy interface---an important concern when the
 * application has to do address arithmetic.
 */
struct hpx_addr {
  __uint128_t addr;                             /**< global virtual address */
};

#define HPX_NULL (struct hpx_addr){0};

#endif /* HPX_AGAS_H_ */
