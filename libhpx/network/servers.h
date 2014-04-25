// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBHPX_NETWORK_SERVERS_H
#define LIBHPX_NETWORK_SERVERS_H

#include "hpx/hpx.h"

HPX_INTERNAL void *heavy_network(void *network);
HPX_INTERNAL extern hpx_action_t light_network;

#endif // LIBHPX_NETWORK_SERVERS_H
