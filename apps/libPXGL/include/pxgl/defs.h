// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBPXGL_DEFS_H
#define LIBPXGL_DEFS_H

#include <inttypes.h>

#ifdef PXGL_64BIT
#define PXGL_UINT_T uint64_t
#define PXGL_UINT_PRI PRIu64
#define PXGL_UINT_MAX UINT64_MAX
#define PXGL_INT_T int64_t
#define PXGL_INT_PRI PRId64
#define PXGL_INT_MAX INT64_MAX
#elif defined(PXGL_32BIT)
#define PXGL_UINT_T uint32_t
#define PXGL_UINT_PRI PRIu32
#define PXGL_UINT_MAX UINT32_MAX
#define PXGL_INT_T int32_t
#define PXGL_INT_PRI PRId32
#define PXGL_INT_MAX INT32_MAX
#endif

typedef PXGL_UINT_T pxgl_uint_t;
typedef PXGL_INT_T pxgl_int_t;

typedef PXGL_UINT_T sssp_uint_t;
typedef PXGL_INT_T sssp_int_t;

typedef PXGL_UINT_T bfs_uint_t;
typedef PXGL_INT_T bfs_int_t;


#endif // LIBPXGL_DEFS_H
