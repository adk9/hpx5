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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/// @file libhpx/network/network.c
#include <libhpx/boot.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/network.h>
#include "isir/isir.h"
#include "pwc/pwc.h"
#include "coalesced.h"
#include "compressed.h"
#include "inst.h"
#include "smp.h"

static const int LEVEL = HPX_LOG_CONFIG | HPX_LOG_NET | HPX_LOG_DEFAULT;

void*
network_new(config_t *cfg, boot_t *boot, struct gas *gas)
{
#ifndef HAVE_NETWORK
  // if we didn't build a network we need to default to SMP
  cfg->network = HPX_NETWORK_SMP;
#endif

  libhpx_network_t type = cfg->network;
  int ranks = boot_n_ranks(boot);
  Network *network = nullptr;

  // default to HPX_NETWORK_SMP for SMP execution
  if (ranks == 1 && cfg->opt_smp) {
    if (type != HPX_NETWORK_SMP && type != HPX_NETWORK_DEFAULT) {
      log_level(LEVEL, "%s overridden to SMP.\n", HPX_NETWORK_TO_STRING[type]);
      cfg->network = HPX_NETWORK_SMP;
    }
    type = HPX_NETWORK_SMP;
  }

  if (ranks > 1) {
#ifndef HAVE_NETWORK
    dbg_error("Launched on %d ranks but no network available\n", ranks);
#endif
  }

  if (ranks > 1 && type == HPX_NETWORK_SMP) {
    dbg_error("SMP network selection fails for %d ranks\n", ranks);
  }

  if (type == HPX_NETWORK_PWC) {
#ifndef HAVE_PHOTON
    dbg_error("PWC network selection fails (photon disabled in config)\n");
#endif
  }

  // handle default
  if (type == HPX_NETWORK_DEFAULT) {
#ifdef HAVE_PHOTON
    type = HPX_NETWORK_PWC;
#else
    type = HPX_NETWORK_ISIR;
#endif
  }

  switch (type) {
   case HPX_NETWORK_PWC:
#ifdef HAVE_NETWORK
    network = static_cast<Network*>(network_pwc_funneled_new(cfg, boot, gas));
    pwc_network = (pwc_network_t*) network;
#else
    log_level(LEVEL, "PWC network unavailable (no network configured)\n");
#endif
    break;

   case HPX_NETWORK_ISIR:
#ifdef HAVE_NETWORK
    network = static_cast<Network*>(network_isir_funneled_new(cfg, boot, gas));
#else
    log_level(LEVEL, "ISIR network unavailable (no network configured)\n");
#endif
    break;

   case HPX_NETWORK_SMP:
    network = static_cast<Network*>(network_smp_new(cfg, boot));
    break;

   default:
    log_level(LEVEL, "unknown network type\n");
    break;
  }

  if (!network) {
    dbg_error("%s did not initialize\n", HPX_NETWORK_TO_STRING[cfg->network]);
  }
  else {
    log_level(LEVEL, "%s network initialized\n", HPX_NETWORK_TO_STRING[type]);
  }

  if (cfg->parcel_compression) {
    network = static_cast<Network*>(compressed_network_new(network));
  }

  if (cfg->coalescing_buffersize) {
    network = static_cast<Network*>(coalesced_network_new(network, cfg));
  }

  if (!config_trace_at_isset(here->config, here->rank)) {
    return network;
  }

  if (!inst_trace_class(HPX_TRACE_NETWORK)) {
    return network;
  }

  return network_inst_new(network);
}

void
network_delete(void *obj)
{
  Network *network = static_cast<Network*>(obj);
  network->deallocate(network);
}

int
network_progress(void *obj, int id)
{
  Network *network = static_cast<Network*>(obj);
  return network->progress(network, id);
}

int
network_send(void *obj, hpx_parcel_t *p, hpx_parcel_t *ssync)
{
  Network *network = static_cast<Network*>(obj);
  return network->send(network, p, ssync);
}

hpx_parcel_t *
network_probe(void *obj, int rank) {
  Network *network = static_cast<Network*>(obj);
  return network->probe(network, rank);
}

void
network_flush(void *obj)
{
  Network *network = static_cast<Network*>(obj);
  return network->flush(network);
}

void
network_register_dma(void *obj, const void *base, size_t bytes, void *key)
{
  Network *network = static_cast<Network*>(obj);
  network->register_dma(network, base, bytes, key);
}

void
network_release_dma(void *obj, const void *base, size_t bytes)
{
  Network *network = static_cast<Network*>(obj);
  network->release_dma(network, base, bytes);
}

int
network_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset)
{
  Network *network = static_cast<Network*>(obj);
  return network->lco_get(network, lco, n, out, reset);
}

int
network_lco_wait(void *obj, hpx_addr_t lco, int reset)
{
  Network *network = static_cast<Network*>(obj);
  return network->lco_wait(network, lco, reset);
}

int
network_memget(void *obj, void *to, hpx_addr_t from, size_t size,
               hpx_addr_t lsync, hpx_addr_t rsync)
{
  Network *network = static_cast<Network*>(obj);
  return network->string->memget(network, to, from, size, lsync, rsync);
}

int
network_memget_rsync(void *obj, void *to, hpx_addr_t from, size_t size,
                     hpx_addr_t lsync)
{
  Network *network = static_cast<Network*>(obj);
  return network->string->memget_rsync(network, to, from, size, lsync);
}

int
network_memget_lsync(void *obj, void *to, hpx_addr_t from, size_t size)
{
  Network *network = static_cast<Network*>(obj);
  return network->string->memget_lsync(network, to, from, size);
}

int
network_memput(void *obj, hpx_addr_t to, const void *from, size_t size,
               hpx_addr_t lsync, hpx_addr_t rsync)
{
  Network *network = static_cast<Network*>(obj);
  return network->string->memput(network, to, from, size, lsync, rsync);
}

int
network_memput_lsync(void *obj, hpx_addr_t to, const void *from, size_t size,
                     hpx_addr_t rsync)
{
  Network *network = static_cast<Network*>(obj);
  return network->string->memput_lsync(network, to, from, size, rsync);
}

int
network_memput_rsync(void *obj, hpx_addr_t to, const void *from, size_t size)
{
  Network *network = static_cast<Network*>(obj);
  return network->string->memput_rsync(network, to, from, size);
}

int
network_memcpy(void *obj, hpx_addr_t to, hpx_addr_t from, size_t size,
               hpx_addr_t sync)
{
  Network *network = static_cast<Network*>(obj);
  // use this call syntax do deal with issues on darwin with the memcpy symbol
  return (*network->string->memcpy)(network, to, from, size, sync);
}

int
network_memcpy_sync(void *obj, hpx_addr_t to, hpx_addr_t from, size_t size)
{
  Network *network = static_cast<Network*>(obj);
  return network->string->memcpy_sync(network, to, from, size);
}
