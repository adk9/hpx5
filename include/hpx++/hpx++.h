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

/// @file include/hpx++/hpx++.h
/// @brief The Main HPX++ header file.
/// To use the HPX++ API only this header is needed; all the other HPX++
/// headers are included through it.

#ifndef HPX_PLUS_PLUS_H
#define HPX_PLUS_PLUS_H

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

extern "C" {
  #include "hpx/attributes.h"
  #include "hpx/types.h"
//   #include "hpx/action.h"
}

// #include "hpx/addr.h"
// #include "hpx/gas.h"
#include "hpx++/action.h"
#include "hpx++/global_ptr.h"

extern "C" {
//   #include "hpx/lco.h"
  #include "hpx/par.h"
  #include "hpx/parcel.h"
  #include "hpx/process.h"
  #include "hpx/rpc.h"
}

#include "hpx++/runtime.h"
#include "hpx++/lco.h"

extern "C" {
  #include "hpx/thread.h"
  #include "hpx/time.h"
  #include "hpx/topology.h"
}

#endif // HPX_PLUS_PLUS_H
