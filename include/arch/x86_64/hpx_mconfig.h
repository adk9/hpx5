
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

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/


#pragma once
#ifndef LIBHPX_MCONFIG_X86_64_H_
#define LIBHPX_MCONFIG_X86_64_H_

#include <stdint.h>
#include "arch/x86_64/hpx_mconfig_defs.h"


/*
 --------------------------------------------------------------------
  Machine Configuration Types
 --------------------------------------------------------------------
*/

typedef uint64_t hpx_mconfig_t;


/*
 --------------------------------------------------------------------
  Machine Configuration Functions
 --------------------------------------------------------------------
*/

hpx_mconfig_t hpx_mconfig_get_flags(void);

#endif
