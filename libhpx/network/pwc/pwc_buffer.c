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
#include <string.h>

#include "hpx/builtins.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "segment.h"
#include "pwc_buffer.h"

typedef struct photon_buffer_priv_t rdma_key_t;

typedef struct {
  void        *rva;
  const void  *lva;
  size_t         n;
  hpx_addr_t lsync;
  hpx_addr_t rsync;
  command_t    cmd;
  rdma_key_t   key;
} record_t;

/// Try to start a pwc.
static int _start(pwc_buffer_t *buffer, void *rva, const void *lva, size_t n,
                  hpx_addr_t lsync, hpx_addr_t rsync, command_t cmd,
                  rdma_key_t key) {
  if (rsync != HPX_NULL) {
    dbg_error("remote complete event currently unsupported");
  }

  int flag = ((lsync) ? 0 : PHOTON_REQ_PWC_NO_LCE) |
             ((cmd) ? 0 : PHOTON_REQ_PWC_NO_RCE);

  // int flags = 0;
  int rank = buffer->rank;
  void *vlva = (void*)lva;
  int e = photon_put_with_completion(rank, vlva, n, rva, key, lsync, cmd, flag);
  switch (e) {
   case PHOTON_OK:
    return LIBHPX_OK;
   case PHOTON_ERROR_RESOURCE:
    return LIBHPX_RETRY;
   default:
    return log_error("could not initiate a put-with-completion\n");
  }
}

/// Used as a function callback in circular_buffer_progress.
static int _start_record(void *buffer, void *record) {
  pwc_buffer_t *b = buffer;
  record_t *r = record;
  void *rva = r->rva;
  const void *lva = r->lva;
  size_t n = r->n;
  hpx_addr_t lsync = r->lsync;
  hpx_addr_t rsync = r->rsync;
  uint64_t cmd = r->cmd;
  rdma_key_t key = r->key;
  return _start(b, rva, lva, n, lsync, rsync, cmd, key);
}

int pwc_buffer_init(pwc_buffer_t *buffer, uint32_t rank, uint32_t size) {
  buffer->rank = rank;
  return circular_buffer_init(&buffer->pending, sizeof(record_t), size);
}

void pwc_buffer_fini(pwc_buffer_t *buffer) {
  circular_buffer_fini(&buffer->pending);
}

int pwc_buffer_put(pwc_buffer_t *buffer, size_t roff, const void *lva, size_t n,
                   hpx_addr_t lsync, hpx_addr_t rsync, command_t cmd,
                   segment_t *segment) {
  void *rva = segment_offset_to_rva(segment, roff);
  rdma_key_t key = segment->key;

  // Before performing this put, try to progress the buffer. The progress call
  // returns the number of buffered requests remaining.
  if (pwc_buffer_progress(buffer) == 0) {
    int e = _start(buffer, rva, lva, n, lsync, rsync, cmd, key);

    if (LIBHPX_OK == e) {
      return LIBHPX_OK;
    }

    if (LIBHPX_RETRY != e) {
      return log_error("buffered put failed\n");
    }
  }

  // Either there were buffered PWCs remaining, or the _start() returned retry,
  // so buffer this pwc for now.
  record_t *r = circular_buffer_append(&buffer->pending);
  if (!r) {
    return log_error("could not allocate a circular buffer record\n");
  }

  r->rva = rva;
  r->lva = lva;
  r->n = n;
  r->lsync = lsync;
  r->rsync = rsync;
  r->cmd = cmd;
  r->key = key;

  return LIBHPX_OK;
}

int pwc_buffer_progress(pwc_buffer_t *buffer) {
  int i = circular_buffer_progress(&buffer->pending, _start_record, buffer);
  dbg_assert_str(i >= 0, "failed to progress the pwc buffer\n");
  return i;
}
