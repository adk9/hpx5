
/*
 ====================================================================
  ParalleX Programming Interface (XPI) Library (libxpi)
  
  Datatype Definitions
  xpi_types.h

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

#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include "xpi/xpi_error.h"

#pragma once
#ifndef LIBXPI_TYPES_H_
#define LIBXPI_TYPES_H_


typedef int                     XPI_Type;


/*
 --------------------------------------------------------------------
  Instrinsic Types
 --------------------------------------------------------------------
*/

typedef XPI_VOID                void;
typedef XPI_ERROR               XPI_Err;
typedef XPI_BOOL                bool;
typedef XPI_CHAR                char;
typedef XPI_SHORT               short;
typedef XPI_INT                 int;
typedef XPI_LONG                long;
typedef XPI_LONG_LONG           long long;
typedef XPI_UNSIGNED_CHAR       unsigned char;
typedef XPI_UNSIGNED_INT        unsigned int;
typedef XPI_UNSIGNED_SHORT      unsigned short;
typedef XPI_UNSIGNED_LONG       unsigned long;
typedef XPI_UNSIGNED_LONG_LONG  unsigned long long;
typedef XPI_FLOAT               float;
typedef XPI_DOUBLE              double;
typedef XPI_LONG_DOUBLE         long double;
typedef XPI_FLOAT_COMPLEX       complex float;
typedef XPI_DOUBLE_COMPLEX      complex double;
typedef XPI_LONG_DOUBLE_COMPLEX complex long double;
typedef XPI_UINT8_T             uint8_t;
typedef XPI_UINT16_T            uint16_t;
typedef XPI_UINT32_T            uint32_t;
typedef XPI_UINT64_T            uint64_t;
typedef XPI_INT8_T              int8_t;
typedef XPI_INT16_T             int16_t;
typedef XPI_INT32_T             int32_t;
typedef XPI_INT64_T             int64_t;
typedef XPI_SIZE_T              size_t;


/*
 --------------------------------------------------------------------
  Derived Types
 --------------------------------------------------------------------
*/

/* XPI_TYPE_CONTIGUOUS */
XPI_Err XPI_Type_contiguous(size_t, XPI_Type, XPI_Type *);

/* XPI_TYPE_VECTOR */
XPI_Err XPI_Type_vector(size_t, size_t, size_t, XPI_Type, XPI_Type *);

/* XPI_TYPE_INDEXED */
XPI_Err XPI_Type_indexed(size_t, size_t, size_t, XPI_Type, XPI_Type *);

/* XPI_TYPE_STRUCT */
XPI_Err XPI_Type_struct(size_t, size_t, size_t, XPI_Type, XPI_Type *);

/* XPI_TYPE_LCO */
XPI_Err XPI_Type_lco(XPI_Type, XPI_Type *);

/* XPI_TYPE_GLOBAL_POINTER */
XPI_Err XPI_Type_global_pointer(XPI_Type, XPI_Type *);

/* XPI_TYPE_ACTION */
XPI_Err XPI_Type_action(size_t, XPI_Type, XPI_Type *);


/*
 --------------------------------------------------------------------
  Utility Functions
 --------------------------------------------------------------------
*/

XPI_Err XPI_Type_get_size(XPI_Type, size_t *);
bool XPI_Type_equals(XPI_Type, XPI_Type);

#endif


