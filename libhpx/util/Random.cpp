// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/// @file libhpx/util/Random.cpp
/// @brief Implement the HPX thread-local random number generator.

#include "libhpx/util/Random.h"

namespace {
thread_local struct Random {
  Random() : rd(), mt(rd()) {
  }
  std::random_device rd;
  std::mt19937 mt;
} rng{};
}

std::mt19937& libhpx::util::getRNG() {
  return rng.mt;
}
