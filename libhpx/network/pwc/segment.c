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

#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "segment.h"

int segment_register(segment_t *segment) {
  void *base = segment->base;
  size_t size = segment->size;

  if (PHOTON_OK != photon_register_buffer(base, size)) {
    return dbg_error("failed to register segment with Photon\n");
  }
  else {
    dbg_log_net("registered segment (%p, %lu)\n", base, size);
  }

  if (PHOTON_OK != photon_get_buffer_private(base, size , &segment->key)) {
    return dbg_error("failed to segment key from Photon\n");
  }

  return LIBHPX_OK;
}

void segment_deregister(segment_t *segment) {
  void *base = segment->base;
  size_t size = segment->size;
  if (PHOTON_OK != photon_unregister_buffer(base, size)) {
    dbg_log_net("could not unregister the local heap segment %p\n", base);
  }
}

