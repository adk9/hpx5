/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  User-Level Parcel Definition
  parcel.h

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

#ifndef LIBHPX_ADDRESS_H_
#define LIBHPX_ADDRESS_H_

#include "hpx/agas.h"                           /* struct hpx_addr */
#include "hpx/runtime.h"                        /* struct hpx_locality */
#include "hpx/system/attributes.h"              /* HPX_MACROS */

typedef struct address address_t;

/**
 * @file @brief A physical address for use in the network.
 */

struct address {
  struct hpx_locality locality;
};

#endif /* LIBHPX_ADDRESS_H_ */
