
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Error Handling Function Definitions
  hpx_error.h

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
#ifndef LIBHPX_ERROR_H_
#define LIBHPX_ERROR_H_

#include <stdint.h>


/*
 --------------------------------------------------------------------
  Error Types
 --------------------------------------------------------------------
*/

typedef uint8_t hpx_error_t;

/* we'll have to make this thread safe some time */
static hpx_error_t __hpx_errno;


/*
 --------------------------------------------------------------------
  Error Functions
 --------------------------------------------------------------------
*/

/*
hpx_error_t * __hpx_error(void);
#define hpx_errno (*__hpx_error());
*/

/*
 --------------------------------------------------------------------
  Error Codes
 --------------------------------------------------------------------
*/

#define HPX_SUCCESS                                           0x0000  /* no error */
#define HPX_ERROR_NOMEM                                       0x0001  /* can't allocate (out of memory) */

#endif


