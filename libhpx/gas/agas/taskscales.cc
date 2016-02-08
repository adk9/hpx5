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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libhpx/libhpx.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include <TaskScales/core.h>
#include <tuple>
#include <iostream>
#include <string>

//#include "taskscales.h"

void taskscales_init(void) {
  auto version = TaskScales::version();
  std::cout << "TaskScales version: "
            << std::get<0>(version) << "."
            << std::get<1>(version) << "."
            << std::get<2>(version) << "\n";
  return;
}
