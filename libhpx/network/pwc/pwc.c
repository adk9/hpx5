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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include <libhpx/debug.h>
#include <libhpx/network.h>

#include "pwc.h"

typedef struct {
  network_t vtable;
} _pwc_t;

static const char *_photon_id() {
  return "Photon put-with-completion\n";
}

network_t *network_pwc_funneled_new(struct gas_class *gas, int nrx) {
  _pwc_t *photon =  malloc(sizeof(*photon));
  if (!photon) {
    dbg_error("could not allocate a Photon put-with-completion network\n");
    return NULL;
  }

  photon->vtable.id = _photon_id;
  photon->vtable.delete = NULL;
  photon->vtable.progress = NULL;
  photon->vtable.send = NULL;
  photon->vtable.pwc = NULL;
  photon->vtable.put = NULL;
  photon->vtable.get = NULL;
  photon->vtable.probe = NULL;
  photon->vtable.set_flush = NULL;

  return &photon->vtable;
}
