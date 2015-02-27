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

#include <string.h>
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "segment.h"

int segment_init(segment_t *segment, char *base, size_t size) {
#ifndef HAVE_PHOTON
  return LIBHPX_ERROR;
#endif

  segment->base = base;
  segment->size = size;
  if (!base) {
    assert(!size);
    memset(&segment->key, 0, sizeof(segment->key));
    return LIBHPX_OK;
  }

  if (PHOTON_OK != photon_register_buffer(base, size)) {
    dbg_error("failed to register segment with Photon\n");
  }
  else {
    log_net("registered segment (%p, %lu)\n", base, size);
  }

  if (PHOTON_OK != photon_get_buffer_private(base, size , &segment->key)) {
    dbg_error("failed to segment key from Photon\n");
  }

  return LIBHPX_OK;
}

void segment_fini(segment_t *segment) {
#ifndef HAVE_PHOTON
  return LIBHPX_ERROR;
#endif

  void *base = segment->base;
  size_t size = segment->size;
  if (!base) {
    return;
  }

  if (PHOTON_OK != photon_unregister_buffer(base, size)) {
    log_net("could not unregister the local heap segment %p\n", base);
  }
}

