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

#ifndef LIBHPX_NETWORK_H
#define LIBHPX_NETWORK_H

/// @file include/libhpx/c_network.h
/// @brief Declare the network_t interface.
///
/// This file declares the interface to the parcel network subsystem in HPX. The
/// network's primary responsibility is to accept send requests from the
/// scheduler, and send them out via the configured transport.
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/string.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
struct transport;
/// @}

typedef void network_t;

int network_coll_init(void *obj, void **collective);
int network_coll_sync(void *obj, void *in, size_t in_size, void* out, void *collective);

int network_memget(void *obj, void *to, hpx_addr_t from, size_t size,
                   hpx_addr_t lsync, hpx_addr_t rsync);

int network_memget_rsync(void *obj, void *to, hpx_addr_t from, size_t size,
                         hpx_addr_t lsync);

int network_memget_lsync(void *obj, void *to, hpx_addr_t from, size_t size);

int network_memput(void *obj, hpx_addr_t to, const void *from, size_t size,
                   hpx_addr_t lsync, hpx_addr_t rsync);

int network_memput_lsync(void *obj, hpx_addr_t to, const void *from,
                         size_t size, hpx_addr_t rsync);

int network_memput_rsync(void *obj, hpx_addr_t to, const void *from,
                         size_t size);

int network_memcpy(void *obj, hpx_addr_t to, hpx_addr_t from, size_t size,
                   hpx_addr_t sync);

int network_memcpy_sync(void *obj, hpx_addr_t to, hpx_addr_t from, size_t size);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_NETWORK_H
