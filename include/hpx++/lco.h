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

#ifndef LCO_H
#define LCO_H

extern "C" {
  #include "hpx/addr.h"
  #include "hpx/gas.h"
  #include "hpx/types.h"
}

#include <cstdlib>

namespace hpx {
  
  /*
   * This class is provides the lco interface common to all types of LCOs
   * extended by all LCO classes such as Future, Reduce, AndGate etc.
   */
  template <typename T>
  class BaseLCO {
    void fini() {
      static_cast<T*>(this)->fini();
    }
    void set(int size, const void *value) {
      static_cast<T*>(this)->set(size, value);
    }
    void error(hpx_status_t code) {
      static_cast<T*>(this)->error(code);
    }
    hpx_status_t get(int size, void *value, int reset) {
      return static_cast<T*>(this)->get(size, value, reset);
    }
    hpx_status_t getref(int size, void **out, int *unpin) {
      return static_cast<T*>(this)->getref(size, out, unpin);
    }
    int release(void *out) {
      return static_cast<T*>(this)->release(out);
    }
    hpx_status_t wait(int reset) {
      return static_cast<T*>(this)->wait(reset);
    }
    hpx_status_t attach(hpx_parcel_t *p) {
      return static_cast<T*>(this)->attach(p);
    }
    void reset() {
      static_cast<T*>(this)->reset();
    }
    std::size_t size() {
      return static_cast<T*>(this)->size();
    }
  };
  
  class Future : public BaseLCO<Future> {
    
  };
  
  class AndGate : public BaseLCO<AndGate> {
  };
  
  class Reduce : public BaseLCO<Reduce> {
    
  };
  class Semaphore : public BaseLCO<Semaphore> {
  };
  
}

#endif
