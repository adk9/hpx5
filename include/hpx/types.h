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
#ifndef HPX_TYPES_H
#define HPX_TYPES_H

/// @file include/hpx/types.h

/// Extern HPX macros
/// @{
#if defined(__x86_64__)
typedef enum {
  HPX_ERROR          = -1,
  HPX_SUCCESS        = 0,
  HPX_RESEND         = 1,
  HPX_LCO_ERROR      = 2,
  HPX_LCO_CHAN_EMPTY = 3,
  HPX_LCO_TIMEOUT    = 4,
  HPX_USER           = 127
} hpx_status_t;
#elif defined( __ARMEL__)
typedef short hpx_status_t;
#define  HPX_ERROR           -1
#define  HPX_SUCCESS         0
#define  HPX_RESEND          1
#define  HPX_LCO_ERROR       2
#define  HPX_LCO_CHAN_EMPTY  3
#define  HPX_LCO_TIMEOUT     4
#define  HPX_USER            127
#endif
/// @}

#endif
