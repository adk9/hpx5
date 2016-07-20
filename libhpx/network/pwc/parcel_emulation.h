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

#ifndef LIBHPX_NETWORK_PWC_PARCEL_EMULATION_H
#define LIBHPX_NETWORK_PWC_PARCEL_EMULATION_H

#ifdef __cplusplus
extern "C" {
#endif

struct boot;
struct config;
struct pwc_xport;

typedef struct parcel_emulator {
  void (*deallocate)(void *obj);
  int (*send)(void *obj, struct pwc_xport *xport, int rank,
              const hpx_parcel_t *p);
  hpx_parcel_t *(*recv)(void *obj, int rank);
} parcel_emulator_t;

parcel_emulator_t *parcel_emulator_new_reload(const struct config *cfg, struct boot *boot,
                                              struct pwc_xport *xport);

static inline void parcel_emulator_deallocate(void *obj) {
  parcel_emulator_t *emulator = (parcel_emulator_t *)obj;
  emulator->deallocate(obj);
}

static inline int parcel_emulator_send(void *obj, struct pwc_xport *xport,
                                       int rank, const hpx_parcel_t *p) {
  parcel_emulator_t *emulator = (parcel_emulator_t *)obj;
  return emulator->send(obj, xport, rank, p);
}

static inline hpx_parcel_t *parcel_emulator_recv(void *obj, int rank) {
  parcel_emulator_t *emulator = (parcel_emulator_t *)obj;
  return emulator->recv(obj, rank);
}

#ifdef __cplusplus
}
#endif

#endif
