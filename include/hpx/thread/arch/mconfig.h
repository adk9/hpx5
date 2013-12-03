/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Machine Configuration Function Definitions
  hpx_mconfig.h

  Copyright (c) 2013,      Trustees of Indiana University 
  Copyright (c) 2002-2012, Intel Corporation
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#ifndef HPX_MCONFIG_H_
#define HPX_MCONFIG_H_

#ifdef __x86_64__
#include "hpx/thread/arch/x86_64/mconfig.h"
#else
#error No machine configuration available for your platform.
#endif


#endif /* HPX_MCONFIG_H_ */
