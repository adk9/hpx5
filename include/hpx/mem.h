
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Shared memory management function definitions
  hpx_mem.h

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
#ifndef LIBHPX_MEM_H_
#define LIBHPX_MEM_H_

#include <stdlib.h>

#define hpx_alloc(u) malloc(u)
#define hpx_free(u)  free(u)

#endif


