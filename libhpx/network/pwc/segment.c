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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "segment.h"
#include "xport.h"

int segment_init(segment_t *segment, pwc_xport_t *xport, char *base,
                 size_t size) {
  dbg_assert(base || !size);
  segment->base = base;
  segment->size = size;
  if (base) {
    xport->pin(base, size, &segment->key);
  }
  else {
    xport->clear(&segment->key);
  }
  return LIBHPX_OK;
}

void segment_fini(segment_t *segment, pwc_xport_t *xport) {
  xport->unpin(segment->base, segment->size);
}

