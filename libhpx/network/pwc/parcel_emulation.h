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
#ifndef LIBHPX_NETWORK_PWC_PARCEL_EMULATION_H
#define LIBHPX_NETWORK_PWC_PARCEL_EMULATION_H

struct boot;
struct config;
struct pwc_xport;

typedef struct {
  void (*delete)(void *obj);
} parcel_emulator_t;

void *parcel_emulator_new_reload(const struct config *cfg, struct boot *boot,
                                 struct pwc_xport *xport)
  HPX_INTERNAL;

static inline void parcel_emulator_delete(void *obj) {
  parcel_emulator_t *emulator = obj;
  emulator->delete(obj);
}

#endif
