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
  #include "hpx/lco.h"
}

#include <cstdlib>

namespace hpx {
  
  namespace lco {
    /*
    * This class is provides the lco interface common to all types of LCOs
    * extended by all LCO classes such as Future, Reduce, AndGate etc.
    */
    template <typename T>
    class BaseLCO {
    
    public:
      BaseLCO() : _size(sizeof(T)) {}
      
      virtual void fini() {
	hpx_lco_delete_sync(_lco);
      }
      virtual void set(int size, const void *value) {
	hpx_lco_delete_sync(_lco);
      }
      virtual void error(hpx_status_t code) {
	hpx_lco_error_sync(_lco, code);
      }
      virtual hpx_status_t get(T& value) {
	return hpx_lco_get(_lco, sizeof(T), &value);
      }
      hpx_status_t getref(T** out) {
	return hpx_lco_getref(_lco, sizeof(T), (void**) out);
      }
      int release(void *out) {
	return hpx_lco_release(_lco, out);
      }
      hpx_status_t wait() {
	return hpx_lco_wait(_lco);
      }
      // TODO not sure what to do here
  //     hpx_status_t attach(hpx_parcel_t *p) {
  //       return static_cast<T*>(this)->attach(p);
  //     }
      
      void reset() {
	hpx_lco_reset_sync(_lco);
      }
      
      std::size_t size() {
	return _size;
      }
      
    protected:
      hpx_addr_t _lco;
      std::size_t _size;
    };

    template <typename T>
    class Future : public BaseLCO<T> {
    public:
      Future() {
	this->_lco = hpx_lco_future_new(sizeof(T));
      }
    };
    
    template <typename T>
    class AndGate : public BaseLCO<T> {
    public:
      AndGate() {}
    };
    
    template <typename T>
    class Reduce : public BaseLCO<T> {
    public:
      Reduce() {}
    };
    
    template <typename T>
    class Semaphore : public BaseLCO<T> {
    public:
      Semaphore() {}
    };
  } // namespace lco
} // namespace hpx

#endif
