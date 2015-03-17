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

#include <libhpx/config.h>
#include <libhpx/boot.h>
#include <libhpx/memory.h>
#include "parcel_emulation.h"
#include "xport.h"

typedef struct {
  size_t   n;
  void *base;
  void  *key;
  int   rank;
  int UNUSED_PADDING;
} segment_t;

typedef struct {
  segment_t segment;
  uint64_t   offset;
} buffer_t;

typedef struct {
  parcel_emulator_t vtable;
  int                 rank;
  int                ranks;
  buffer_t           *recv;
  buffer_t           *send;
  segment_t       *remotes;
} reload_t;

static void _reload_delete(void *obj) {
  if (obj) {
    local_free(obj);
  }
}

void *parcel_emulator_new_reload(const config_t *cfg, boot_t *boot,
                                 pwc_xport_t *xport) {
  int rank = boot_rank(boot);
  int ranks = boot_n_ranks(boot);

  // Allocate the buffer.
  reload_t *reload = local_calloc(1, sizeof(*reload));
  reload->vtable.delete = _reload_delete;
  reload->rank = rank;
  reload->ranks = ranks;

  // Allocate and initialize my initial recv buffers.
  reload->recv = local_calloc(ranks, sizeof(*reload->recv));
  for (int i = 0, e = ranks; i < e; ++i) {
    int n = cfg->pwc_parcelbuffersize;
    segment_t *segment = &reload->recv[i].segment;
    segment->n = n;
    segment->base = registered_calloc(1, n);
    segment->key = xport->find_key(xport, segment->base);
    segment->rank = i;
  }

  // Allocate my array of send buffers and do an initial all-to-all to exchange
  // the segments allocated above: send[j][i] = recv[i][j].
  reload->send = registered_calloc(ranks, sizeof(*reload->send));

  int n = sizeof(segment_t);
  int stride = sizeof(buffer_t);
  boot_alltoall(boot, reload->send, reload->recv, n, stride);

  // Create a segment for my element in the remotes (i.e., the segment
  // corresponding to my send buffer row which peers use to update my send
  // buffer to point to their recv buffer) allreduce the
  segment_t sends = {
    .n = sizeof(segment_t) * ranks,
    .base = reload->remotes,
    .key = xport->find_key(xport, reload->remotes),
    .rank = rank
  };

  // Allocate the remotes array, and exchange all of the sends segments.
  reload->remotes = local_calloc(ranks, sizeof(*reload->remotes));
  boot_allgather(boot, &sends, reload->remotes, sizeof(sends));
  dbg_assert(reload->remotes[rank].key == sends.key);

  return reload;
}
