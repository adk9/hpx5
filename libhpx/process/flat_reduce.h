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
#ifndef LIBHPX_PROCESS_FLAT_REDUCE_H
#define LIBHPX_PROCESS_FLAT_REDUCE_H

#include <hpx/hpx.h>


/// Determine how many bytes are necessary to allocate a flat reduce.
///
/// The flat reduce structure is variable-sized based on the size of the
/// to-be-reduced value type, and system-specific details. This function allows
/// clients to allocate enough space for one in-place in a dynamically sized
/// buffer.
///
/// The allocated buffer can be initialized asynchronously with the
/// flat_reduce_init action.
///
/// @param        bytes The size of the type to be reduced.
///
/// @returns            The number of bytes that need to be allocated.
size_t flat_reduce_size(size_t bytes);

/// An action for initializing a flat reduce.
///
/// The initialization action is a typed action with the following signature.
///
///  (int n, size_t bytes, hpx_action_t id, hpx_action_t op)
///
/// @param            n The number of inputs to the reduce.
/// @param        bytes The size, in bytes, of the value being reduced.
/// @param           id A registered function for initializing the value.
/// @param           op The commutative-associative reduction operation.
extern HPX_ACTION_DECL(flat_reduce_init);

/// An action for finalizing a flat reduce.
extern HPX_ACTION_DECL(flat_reduce_fini);

/// Join in a flat reduction.
///
/// This join operation is synchronous, and will be "done" with @p value when it
/// returns, thus it is safe to free or modify after the call. When the
/// reduction is complete it will return non-zero and copy the reduced value to
/// @p out. If the reduction is not complete it will return 0 and will not
/// modify @p out. The @p value and @p out pointers may alias, the reduction
/// will consume @p value before writing to @p out.
///
/// The reduction will be reset synchronously with the last arriving joiner, and
/// will only provide the reduced value that one time.
///
/// The reduction will interpret @p value and @p out as buffers of at least the
/// size with which the reduce was initialized.
///
/// @param          obj The reduce object pointer.
/// @param        value The value to merge into the reduction.
/// @param[out]     out The output buffer.
///
/// @returns            1, the @p out buffer contains the reduced value
///                     0, the reduction is not yet complete
int flat_reduce_join(void *obj, const void *value, void *out);

#endif // LIBHPX_PROCESS_FLAT_REDUCE_H
