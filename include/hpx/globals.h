/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library initialization and cleanup function definitions
  hpx_init.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/

#pragma once
#ifndef HPX_GLOBALS_H_
#define HPX_GLOBALS_H_

/**
 * @file
 * @brief Expose any global variables that the HPX application developer needs
 *        to know about here.
 *
 * It will ultimately be our goal to have no global variables exposed to
 * application developers, the current need for this file is just a side-effect
 * of the initial HPX design.
 */

/**
 * @brief Forward declarations where we can get away with it. These types are
 *        only exposed to the application through pointers.
 * @{
 */
struct hpx_config;
struct hpx_context;
struct network_ops;
struct bootstrap_ops;
/**
 * @}
 */

/**
 * @brief The global variables.
 *
 * These should neither be exposed like this, nor be prefixed with a __.
 * @{
 */
extern struct hpx_config *__hpx_global_cfg;
extern struct hpx_context *__hpx_global_ctx;
extern struct network_ops *__hpx_network_ops;
extern struct bootstrap_ops *bootmgr;
/**
 * @}
 */

#endif /* HPX_GLOBALS_H_ */
