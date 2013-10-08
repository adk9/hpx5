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

#include <config.h>                             /* WITH_OPENSSL */
#include <string.h>                             /* strlen */
#include <stdio.h>                              /* fprintf */
#ifdef WITH_OPENSSL
#include "openssl/md2.h"                        /* MD5 */
#endif
#include "hashstr.h"

static const int MAX_STR_LEN = 1024;

static void warn_len(const int len, const char * const str) {
  fprintf(stderr, "Registering an action with a long key (%u character "
                  "limit, truncating to:\n", len);
  for (int i = 0; i < len; ++i)
    fprintf(stderr, "%c", str[i]);
  fprintf(stderr, "\n");
}


/*
 * Hash the passed string. If we have OPENSSL available, then we just
 * use md5 to compute a 128 bit hash, and select part of it as our hash.
 * Otherwise, we use a custom (probably less well distributed) hash.
 */
uintptr_t libhpx_hash_str_uip(char *str) {
  const size_t len = strnlen(str, MAX_STR_LEN);
  if (len == MAX_STR_LEN)
    warn_len(len, str);

#ifdef WITH_OPENSSL
  uintptr_t md[16 / sizeof(uintptr_t)];         /* space for 128 bits */
  MD5(str, len, md);                            /* hash */
  return md[0];                                 /* just pick some subset */
#else
#error No custom hash string available yet.
  return 0;
#endif
}
