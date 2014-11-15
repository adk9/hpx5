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
#include <config.h>
#endif

#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <pmi.h>

#include "libhpx/boot.h"
#include "libhpx/locality.h"
#include "libhpx/debug.h"


static HPX_RETURNS_NON_NULL const char *_id(void) {
  return "PMI";
}


static void _delete(boot_class_t *boot) {
  PMI_Finalize();
}


static int _rank(const boot_class_t *boot) {
  int rank;
  if (PMI_Get_rank(&rank) != PMI_SUCCESS)
    hpx_abort();
  return rank;
}


static int _n_ranks(const boot_class_t *boot) {
  int ranks;
  if (PMI_Get_size(&ranks) != PMI_SUCCESS)
    hpx_abort();
  return ranks;
}


static int _barrier(const boot_class_t *boot) {
  return (PMI_Barrier() != PMI_SUCCESS) ? HPX_ERROR : HPX_SUCCESS;
}


static void _abort(const boot_class_t *boot) {
  PMI_Abort(-6, "HPX aborted.");
}


/// ----------------------------------------------------------------------------
/// Base64 string encoding.
///
/// todo: Cray PMI does not allow characters such as /;=. We need to
/// make the encoding function account for that.
/// ----------------------------------------------------------------------------
static int
_encode(const void *src, size_t slen, char *dst, size_t *dlen) {
  assert ((dlen != NULL) || (dst != NULL));
  unsigned char *s = (unsigned char*)src;
  char *d = (char*)dst;
  char *p;

  *dlen = ((slen+3)/4)*6+1;
  uint64_t w;
  while (slen > 3) {
    w = *s++; w = (w << 8) | *s++; w = (w << 8) | *s++; w = (w << 8) | *s++;
    slen -= 4;
    p = stpcpy(d, l64a(htonl(w)));
    d = mempcpy(p, "......", 6-(p-d));
  }

  if (slen > 0) {
    w = *s++;
    if (--slen > 0) {
      w = (w << 8) | *s++;
      if (--slen > 0)
        w = (w << 8) | *s;
    }
    d = stpcpy(d, l64a(htonl(w)));
  }
  *d = '\0';
  return HPX_SUCCESS;
}

/// ----------------------------------------------------------------------------
/// Base64 string decoding. The length of source and destination
/// buffers is always unequal due to padding.
/// ----------------------------------------------------------------------------
static int
_decode(const char *src, size_t slen, void *dst, size_t dlen) {
  assert (dst != NULL);
  char *s = (char*)src;
  unsigned char *d = (unsigned char*)dst;

  assert(dlen >= (2*slen-11)/3);
  long w;
  for (int i = 0, j = 0; i < slen; j+=4, i+=6) {
    w = ntohl(a64l(s+i));
    d[j+3] = w & 0xFF;
    w = (w >> 8);
    d[j+2] = w & 0xFF;
    w = (w >> 8);
    d[j+1] = w & 0xFF;
    w = (w >> 8);
    d[j] = w & 0xFF;
  }
  // handle left-over?

  return HPX_SUCCESS;
}

/// ----------------------------------------------------------------------------
/// This publishes a buffer in PMI's key-value store. The buffer
/// is associated with our rank as its key. Since the value needs to
/// be a string buffer, it is first encoded using base64 encoding
/// before doing a KVS put.
/// ----------------------------------------------------------------------------
static int HPX_USED _put_buffer(char *kvs, int rank, void *buffer, size_t len)
{
  int length;

  // allocate key
  int e = PMI_KVS_Get_key_length_max(&length);
  if (e != PMI_SUCCESS)
    return dbg_error("pmi: failed to get max key length.\n");
  char *key = malloc(sizeof(*key) * length);
  snprintf(key, length, "%d", rank);

  // allocate value
  e = PMI_KVS_Get_value_length_max(&length);
  if (e != PMI_SUCCESS) {
    free(key);
    return dbg_error("pmi: failed to get max value length.\n");
  }
  char *value = malloc(sizeof(*value) * length);
  if ((_encode(buffer, len, value, (size_t*)&length)) != HPX_SUCCESS)
    goto error;

  // do the key-value pair exchange
  if ((PMI_KVS_Put(kvs, key, value)) != PMI_SUCCESS)
    goto error;

  if ((PMI_KVS_Commit(kvs)) != PMI_SUCCESS)
    goto error;

  PMI_Barrier();

  free(key);
  free(value);
  return length;

error:
  free(key);
  free(value);
  return dbg_error("pmi: failed to put buffer in PMI's KVS.\n");
}

/// ----------------------------------------------------------------------------
/// This retrieves a buffer from PMI's key-value store. The buffer is
/// base64 decoded before it is returned.
/// ----------------------------------------------------------------------------
static int HPX_USED _get_buffer(char *kvs, int rank, void *buffer, size_t len)
{
  int length;

  // allocate key
  int e = PMI_KVS_Get_key_length_max(&length);
  if (e != PMI_SUCCESS)
    return dbg_error("pmi: failed to get max key length.\n");
  char *key = malloc(sizeof(*key) * length);
  snprintf(key, length, "%d", rank);

  // allocate value
  e = PMI_KVS_Get_value_length_max(&length);
  if (e != PMI_SUCCESS) {
    free(key);
    return dbg_error("pmi: failed to get max value length.\n");
  }
  char *value = malloc(sizeof(*value) * length);
  if ((PMI_KVS_Get(kvs, key, value, length)) != PMI_SUCCESS)
    goto error;

  if ((_decode(value, strlen(value), buffer, len)) != HPX_SUCCESS)
    goto error;

  free(key);
  free(value);
  return HPX_SUCCESS;

error:
  free(key);
  free(value);
  return dbg_error("pmi: failed to put buffer in PMI's KVS.\n");
}


static int _allgather(const boot_class_t *boot, void *in, void *out, int n) {
#if HAVE_PMI_CRAY_EXT
  // nb: Cray PMI allgather does not guarantee process-rank
  // order. Here, we assume that the ordering is, at least,
  // deterministic for all allgather invocations.

  int *nranks = malloc(sizeof(*nranks) * here->ranks);
  if ((PMI_Allgather(&here->rank, nranks, here->ranks)) != PMI_SUCCESS) {
    free(nranks);
    return dbg_error("pmi: failed in PMI_Allgather.\n");
  }

  void *buf = malloc(sizeof(char) * n * here->ranks);
  assert(buf != NULL);
  if ((PMI_Allgather(in, buf, n)) != PMI_SUCCESS) {
    free(buf);
    free(nranks);
    return dbg_error("pmi: failed in PMI_Allgather.\n");
  }

  for (int i = 0; i < here->ranks; i++)
    memcpy((char*)out+(nranks[i]*n), (char*)buf+(i*n), n);

  free(buf);
  free(nranks);
#else
  int length;

  // allocate name for the nidpid map exchange
  int e = PMI_KVS_Get_name_length_max(&length);
  if (e != PMI_SUCCESS)
    return dbg_error("pmi: failed to get max name length.\n");
  char *name = malloc(sizeof(*name) * length);
  e = PMI_KVS_Get_my_name(name, length);
  if (e != PMI_SUCCESS) {
    free(name);
    return dbg_error("pmi: failed to get kvs name.\n");
  }

  _put_buffer(name, here->rank, (void*)in, n);

  for (int r = 0; r < here->ranks; r++)
    _get_buffer(name, r, (char*)out+(r*n), n);

#endif
  return HPX_SUCCESS;
}


static boot_class_t _pmi = {
  .type      = HPX_BOOT_PMI,
  .id        = _id,
  .delete    = _delete,
  .rank      = _rank,
  .n_ranks   = _n_ranks,
  .barrier   = _barrier,
  .allgather = _allgather,
  .abort     = _abort
};


boot_class_t *boot_new_pmi(void) {
  int init;
  PMI_Initialized(&init);
  if (init)
    return &_pmi;

  int spawned;
  if (PMI_Init(&spawned) == PMI_SUCCESS) {
    dbg_log_boot("initialized PMI boostrapper.\n");
    return &_pmi;
  }

  return NULL;
}
