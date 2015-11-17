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

#ifndef ACTION_H
#define ACTION_H

extern "C" {
  #include "hpx/types.h"
  #include "hpx/action.h"
}

#include <cstdlib>

namespace hpx {
  
  namespace action {
    
    template <typename F>
    inline
    int register_action(F func) {
      
      // TODO analyze F and call HPX_REGISTER_ACTION
      
      return HPX_SUCCESS;
    }
    
    
  }
}

#endif