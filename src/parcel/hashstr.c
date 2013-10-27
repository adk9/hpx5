/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

/*
  ====================================================================
  Implements string hashing.
  ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>                             /* HAVE_CRYPTO */
#endif

#include <stdint.h>                             /* uintptr_t */
#include <string.h>                             /* strlen */
#include <stdio.h>                              /* fprintf */

#ifdef HAVE_CRYPTO                              /* MD5 */
#include "openssl/md5.h"
#endif

#ifdef HAVE_GLIB
#include "glib.h"                               /* g_quark_from_string */
#endif

#include "hashstr.h"

static const int MAX_STR_LEN = 1024;

static void
warn_len(const int len, const char * const str)
{
  fprintf(stderr, "Registering an action with a long key (%u character "
                  "limit, truncating to:\n", len);
  for (int i = 0; i < len; ++i)
    fprintf(stderr, "%c", str[i]);
  fprintf(stderr, "\n");
}

/**
 * Hash the passed string.
 *
 * Prefer glib GQuark-based hashing. If that's not available, then fall back to
 * using MD5 from libcrypto. If that's not available, then we'll generate an error.
 * 
 */
const uintptr_t
hashstr(const char * const str)
{
  const size_t len = strnlen(str, MAX_STR_LEN);
  if (len == MAX_STR_LEN)
    warn_len(len, str);
  
#if defined(HAVE_GLIB)
  union {
    uintptr_t word;
    GQuark    quarks[sizeof(uintptr_t)/sizeof(GQuark)];
  } md = {0};
  md.quarks[0] = g_quark_from_string(str);
  return md.word;
#elif defined(HAVE_CRYPTO)
  union {
    uint8_t bytes[16];
    uintptr_t words[sizeof(uint8_t[16])/sizeof(uintptr_t)]; 
  } md = {0};                                   /* space for 128 bits */
  MD5(str, len, md.bytes);
  return md.words[0];                           /* just pick some subset */
#else
#error No custom hash string available yet.
  return 0;
#endif
}
