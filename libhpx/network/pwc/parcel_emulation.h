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

#ifndef LIBHPX_NETWORK_PWC_PARCEL_EMULATION_H
#define LIBHPX_NETWORK_PWC_PARCEL_EMULATION_H

extern "C" {
struct boot;
struct config;
struct hpx_parcel;
}

namespace libhpx {
namespace network {
namespace pwc {

struct pwc_xport_t;

struct parcel_emulator_t {
  void (*deallocate)(void *obj);
  int (*send)(void *obj, pwc_xport_t *xport, unsigned rank,
              const struct hpx_parcel *p);
  struct hpx_parcel *(*recv)(void *obj, unsigned rank);
};

parcel_emulator_t *
parcel_emulator_new_reload(const struct config *cfg, struct boot *boot,
                           pwc_xport_t *xport);

static inline void
parcel_emulator_deallocate(void *obj)
{
  parcel_emulator_t *emulator = (parcel_emulator_t *)obj;
  emulator->deallocate(obj);
}

static inline int
parcel_emulator_send(void *obj, pwc_xport_t *xport, unsigned rank,
                     const struct hpx_parcel *p)
{
  parcel_emulator_t *emulator = (parcel_emulator_t *)obj;
  return emulator->send(obj, xport, rank, p);
}

static inline struct hpx_parcel *
parcel_emulator_recv(void *obj, unsigned rank)
{
  parcel_emulator_t *emulator = (parcel_emulator_t *)obj;
  return emulator->recv(obj, rank);
}

} // namespace pwc
} // namespace network
} // namespace libhpx

#endif
