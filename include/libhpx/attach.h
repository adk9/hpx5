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

#ifndef LIBHPX_ATTACH_H
#define LIBHPX_ATTACH_H

/// Attach a parcel as an LCO continuation.
///
/// When the LCO is triggered, all of its waiting threads will be restarted, and
/// all of its attached parcels will be sent to their destinations.
///
/// This is asynchronous, i.e., it will possibly return before the parcel has
/// been attached. Users may supply an LCO for synchronization.
///
/// @target             The lco.
/// @args             p The parcel to attach.
///
/// @continues          HPX_SUCCESS or an error code
extern hpx_action_t attach;

#endif
