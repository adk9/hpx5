/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Some type aliases
  types.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_TYPES_H_
#define LIBHPX_TYPES_H_

#include <stdint.h>

/*
 --------------------------------------------------------------------
  Some handy type aliases.
  --------------------------------------------------------------------
*/
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;

typedef struct hpx_context          hpx_context_t;
typedef struct hpx_future           hpx_future_t;
typedef struct hpx_kthread          hpx_kthread_t;
typedef struct hpx_parcel           hpx_parcel_t;
typedef struct hpx_thread           hpx_thread_t;
typedef struct hpx_thread_reusable  hpx_thread_reusable_t;
typedef struct hpx_mctx_context     hpx_mctx_context_t;
typedef struct hpx_addr             hpx_addr_t;
typedef struct hpx_distribution     hpx_distribution_t;

#endif /* LIBHPX_TYPES_H_ */
