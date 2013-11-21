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
#ifndef LIBHPX_INIT_H_
#define LIBHPX_INIT_H_

/**
 * @file
 * @brief Put all of the libhpx initialization routines in this header. Then, in
 *        modules that need to be initialized, include this header and implement
 *        the appropriate initialization. Finally, update the global initializer
 *        definition, hpx_init(), in order to call your initialization in the
 *        right order.
 */

#include "hpx/error.h"                          /* hpx_error_t */
 
/**
 * Initialize the parcel subsystem.
 *
 * @todo What errors can this produce and what are we supposed to do about it?
 */
hpx_error_t hpx_parcel_init(void);

/**
 * Clean up the parcel subsystem.
 */
void hpx_parcel_cleanup(void);

/**
 * Initialize the context subsystem (contexts contain threads, processes, etc).
 */
void libhpx_ctx_init();

/**
 * Initialize the thread subsystem.
 */
void libhpx_thread_init();

#endif /* LIBHPX_INIT_H_ */
