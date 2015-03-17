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
  int           rank;
  int          ranks;
  buffer_t     *recv;
  buffer_t     *send;
  segment_t *remotes;
} reload_t;

reload_t *parcel_buffer_new_reload(config_t *cfg, boot_t *boot, pwc_xport_t *x)
{
  int rank = boot_rank(boot);
  int ranks = boot_n_ranks(boot);

  // Allocate the buffer.
  reload_t *reload = local_calloc(1, sizeof(*reload));
  reload->rank = rank;
  reload->ranks = ranks;

  // Allocate and initialize my initial recv buffers.
  reload->recv = local_calloc(ranks, sizeof(*reload->recv));
  for (int i = 0, e = ranks; i < e; ++i) {
    reload->recv[i].n = cfg->parcelbuffersize;
    reload->recv[i].base = registered_malloc(1, cfg->parcelbuffersize);
    reload->recv[i].key = x->find_key(x, buffer);
    reload->recv[i].rank = i;
  }

  // Allocate my array of send buffers.
  reload->send = registered_calloc(ranks, sizeof(*reload->send));

  // Perform an initial all-to-all to exchange all of the send-recv matrix.


  // Create a segment for my element in the remotes
  segment_t s = {
    .n = sizeof(segment_t) * ranks,
    .base = reload->remotes,
    .key = x->find_key(x, reload->remote),
    .rank = rank
  };

  // Exchange remotes with everyone

  return buffer;
}
