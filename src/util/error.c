
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Error Handling Functions
  hpx_error.c

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


#include "hpx/error.h"


/*
 --------------------------------------------------------------------
  __hpx_error
 --------------------------------------------------------------------
*/

hpx_error_t __hpx_error(void) {
  return __hpx_errno;
}
