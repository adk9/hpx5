// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBHPX_ARCH_H
#define LIBHPX_ARCH_H

#if SIZEOF_VOID_P==8
  #define HPX_BITNESS_64
#else
  #define HPX_BITNESS_32
#endif

#endif // define LIBHPX_ARCH_H
