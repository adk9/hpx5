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
#ifndef LIBHPX_NETWORK_PROBE_H
#define LIBHPX_NETWORK_PROBE_H

/// Forward declarations.
/// @{
struct network;
/// @}

int probe_start(struct network *network);
void probe_stop(void);

#endif // LIBHPX_NETWORK_PROBE_H
