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

#pragma once
#ifndef LIBHPX_PARCEL_HASHSTR_H_
#define LIBHPX_PARCEL_HASHSTR_H_

/*
  ====================================================================
  This file provides a local interface to hash a string. It's
  currently used primarily as part of the action interface.
  ====================================================================
*/

#include <stddef.h>                             /* size_t */
#include <stddef.h>                             /* uintptr_t */

/*
 * Hashes the passed, NULL-terminated string into a word-sized
 * integer, suitable for use in a hash table. This is not optimized
 * for speed, so should be used sparingly.
 */
uintptr_t libhpx_hash_str_uip(char *str);

#endif /* LIBHPX_PARCEL_HASHSTR_H_ */
