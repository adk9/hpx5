// ==================================================================-*- C++ -*-
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

#ifndef LIBHPX_NETWORK_PWC_CIRCULAR_BUFFER_H
#define LIBHPX_NETWORK_PWC_CIRCULAR_BUFFER_H

#include <cstdint>

namespace libhpx {
namespace network {
namespace pwc {

class CircularBuffer {
 public:
  /// Allocate a circular buffer.
  CircularBuffer();
  CircularBuffer(unsigned esize, unsigned cap);

  /// Delete a circular buffer.
  ~CircularBuffer();

  /// Initialize a circular buffer.
  ///
  /// @param        esize The size of a buffer element.
  /// @param          cap The initial buffer capacity.
  void init(unsigned esize, unsigned cap);

  /// Finalize a circular buffer.
  void fini();

  /// Append an element to the circular buffer.
  ///
  /// @param            b The buffer to append to.
  ///
  /// @returns       NULL There was an error appending to the buffer.
  ///            NON-NULL The address of the start of the next record in the
  ///                       buffer.
  void *append();

  /// Compute the number of elements in the buffer.
  unsigned size() const;

  /// Apply the callback closure to each element in the buffer, in a FIFO order.
  ///
  ///
  /// @returns            The number of elements left in the buffer.
  int progress(int (*progress_callback)(void *env, void *record),
               void *progress_env);

 private:


  /// Compute the index into the buffer for an abstract index.
  ///
  /// The size must be 2^k because we mask instead of %.
  ///
  /// @param            i The abstract index.
  /// @param     capacity The capacity.
  ///
  /// @returns             @p capacity = 2^k; @p i % @p capacity
  ///                      @p capacity != 2^k; undefined
  static unsigned getIndexOf(unsigned i, unsigned capacity);

  /// Compute the index into the buffer for an abstract value using the current
  /// capacity.
  ///
  /// @param            i The abstract index.
  unsigned getIndexOf(unsigned i) const;

  /// Compute the address of an element in the buffer.
  ///
  /// @param            i The element index.
  ///
  /// @returns            The address of the element in the buffer.
  void *getAddressOf(unsigned i) const;

  /// Reflow the elements in a buffer.
  ///
  /// After expanding a buffer, existing elements are likely to be in the wrong
  /// place, given the new size. This function will reflow the part of the buffer
  /// that is in the wrong place.
  ///
  /// @param  oldCapacity The size of the buffer prior to the expansion.
  void reflow(unsigned oldCapacity);

  /// Expand the capacity of a buffer.
  ///
  /// @param     capacity The new size for the buffer.
  void expand(unsigned capacity);

  unsigned    capacity_;
  unsigned elementSize_;
  uint64_t         max_;
  uint64_t         min_;
  void        *records_;
};

} // namespace pwc
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_PWC_CIRCULAR_BUFFER_H
