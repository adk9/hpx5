
/*
 ====================================================================
  ParalleX Programming Interface (XPI) Library (libxpi)
  
  Error Handling Function Definitions
  xpi_error.h

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
#ifndef LIBXPI_ERROR_H_
#define LIBXPI_ERROR_H_


/*
 --------------------------------------------------------------------
  Error Codes
 --------------------------------------------------------------------
*/

#define XPI_SUCCESS                                           0x0000  /* no error */
#define XPI_ERR_TYPE                                          0x0001  /* type error */
#define XPI_ERR_PARCEL                                        0x0002  /* parcel error */
#define XPI_ERR_NOMEM                                         0x0003  /* out of memory */
#define XPI_ERR_ADDR                                          0x0004  /* bad address */


/*
 --------------------------------------------------------------------
  Error Types
 --------------------------------------------------------------------
*/

typedef uint16_t XPI_Err;

#endif


