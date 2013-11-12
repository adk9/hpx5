/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  An hpx-compatible implementation of stdlib.h abort().
  
  include/libhpx/system/abort.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
  Luke Dalessandro <ldalessa [at] indiana.edu>
  ====================================================================
*/

#ifndef HPX_SYSTEM_ABORT_H_
#define HPX_SYSTEM_ABORT_H_

/**
 * @file
 * @brief Provides stdlib.h-ish abort() in hpx.
 */

#include "hpx/system/attributes.h"

void hpx_abort(void) HPX_ATTRIBUTE(HPX_NORETURN);

#endif /* HPX_SYSTEM_ABORT_H_ */
