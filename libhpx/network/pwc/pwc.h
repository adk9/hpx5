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

#ifndef LIBHPX_NETWORK_PWC_PWC_H
#define LIBHPX_NETWORK_PWC_PWC_H

#include "libhpx/padding.h"
#include "libhpx/parcel.h"

/// Forward declarations.
/// @{
extern "C" {
struct boot;
struct config;
struct gas;
}
/// @}

namespace libhpx {
namespace network {
namespace pwc {

class Command;
struct heap_segment_t;
struct pwc_xport_t;
struct parcel_emulator_t;
struct send_buffer_t;
struct headp_segment_t;

struct pwc_network_t {
  pwc_xport_t            *xport;
  parcel_emulator_t    *parcels;
  send_buffer_t   *send_buffers;
  heap_segment_t *heap_segments;
  PAD_TO_CACHELINE(4 * sizeof(void*));
  volatile int probe_lock;
  PAD_TO_CACHELINE(sizeof(int));
  volatile int progress_lock;
  PAD_TO_CACHELINE(sizeof(int));
};

extern pwc_network_t *pwc_network;

/// Allocate and initialize a PWC network instance.
pwc_network_t *network_pwc_funneled_new(const struct config *cfg,
                                        struct boot *boot, struct gas *gas)
  HPX_MALLOC;

void pwc_deallocate(void *network);
void pwc_flush(void *pwc);
int pwc_send(void *network, hpx_parcel_t *p, hpx_parcel_t *ssync);
int pwc_coll_sync(void *network, void *in, size_t in_size, void *out,
                  void *ctx);
int pwc_coll_init(void *network, void **c);
void pwc_release_dma(void *network, const void* base, size_t n);
void pwc_register_dma(void *network, const void *base, size_t n, void *key);
hpx_parcel_t * pwc_probe(void *network, int rank);
void pwc_progress(void *network, int id);

/// Perform an LCO wait operation through the PWC network.
///
/// This performs a wait operation on a remote LCO. It allows the calling thread
/// to wait without an intermediate "proxy" future allocation.
///
/// @param      network The network object pointer.
/// @param          lco The global address of the LCO to wait for.
/// @param        reset True if the wait should also reset the LCO.
///
/// @returns            The status set in the LCO.
int pwc_lco_wait(void *network, hpx_addr_t lco, int reset);

/// Perform an LCO get operation through the PWC network.
///
/// This performs an LCO get operation on a remote LCO. It allows the calling
/// thread to get an LCO value without having to allocate an intermediate
/// "proxy" future and its associated redundant copies.
///
/// @param          obj The network object pointer.
/// @param          lco The global address of the LCO to wait for.
/// @param            n The size of the output buffer, in bytes.
/// @param          out The output buffer---must be from registered memory.
/// @param        reset True if the wait should also reset the LCO.
///
/// @returns            The status set in the LCO.
int pwc_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset);

/// Perform a rendezvous parcel send operation.
///
/// For normal size parcels, we use the set of one-to-one pre-allocated eager
/// parcel buffers to put the parcel data directly to the target rank. For
/// larger parcels that will either always overflow the eager buffer, or that
/// will use them up quickly and cause lots of re-allocation synchronization, we
/// use this rendezvous protocol.
///
/// @param      network The network object pointer.
/// @param            p The parcel to send.
///
/// @returns            The status of the operation.
int pwc_rendezvous_send(void *network, const hpx_parcel_t *p);

/// Initiate an rDMA get operation.
///
/// This will copy @p n bytes between the @p from buffer and the @p lva, running
/// the @p rcmd when the read completes remotely and running the @p lcmd when
/// the local write is complete.
///
/// @param          obj The network instance to use.
/// @param          lva The local target for the get.
/// @param         from The global source for the get.
/// @param            n The number of bytes to get.
/// @param         lcmd A local command, run when @p lva is written.
/// @param         rcmd A remote command, run when @p from has been read.
///
/// @returns            LIBHPX_OK
int pwc_get(void *obj, void *lva, hpx_addr_t from, size_t n,
            const Command& lcmd, const Command& rcmd);

/// Initiate an rDMA put operation with a remote continuation.
///
/// This will copy @p n bytes between the @p lca and the @p to buffer, running
/// the @p lcmd when the local buffer can be modified or deleted and the @p rcmd
/// when the remote write has completed.
///
/// @param          obj The transport instance to use.
/// @param           to The global target for the put.
/// @param          lva The local source for the put.
/// @param            n The number of bytes to put.
/// @param         lcmd The local command, run when @p lva can be reused.
/// @param         rcmd The remote command, run when @p to has be written.
///
/// @returns            LIBHPX_OK
int pwc_put(void *obj, hpx_addr_t to, const void *lva, size_t n,
            const Command& lcmd, const Command& rcmd);

/// Perform a PWC network command.
///
/// This sends a "pure" command to the scheduler at a different rank, without
/// any additional argument data. This can avoid using any additional eager
/// parcel buffer space, and can always be satisfied with one low-level "put"
/// operation.
///
/// @param          obj The network object pointer.
/// @param         rank The rank id to send the remote command to.
/// @param         lcmd A command to be run for local completion.
/// @param         rcmd A remote command to be run at @p rank.
///
/// @returns            The (local) status of the put operation.
int pwc_cmd(void *obj, int rank, const Command& lcmd, const Command& rcmd);

} // namespace pwc
} // namespace network
} // namespace libhpx

#endif
