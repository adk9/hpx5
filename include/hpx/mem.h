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


#ifdef HAVE_TCMALLOC
#include <google/tcmalloc.h>
#define hpx_alloc_align(u, a, s) tc_posix_memalign(u, a, s)
#define hpx_alloc(u) tc_malloc(u)
#define hpx_realloc(u, s) tc_realloc(u, s)
#define hpx_free(u) tc_free(u)
#else
#define hpx_alloc_align(u, a, s) posix_memalign(u, a, s)
#define hpx_alloc(u) malloc(u)
#define hpx_realloc(u, s) realloc(u, s)
#define hpx_free(u)  free(u)
#endif

#endif /* LIBHPX_MEM_H_ */


