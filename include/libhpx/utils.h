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

#ifndef LIBHPX_UTILS_H
#define LIBHPX_UTILS_H

/// Hash a string @p str of length @p len.
uint32_t libhpx_hash_string(const char *str, size_t len);

/// Get a value from the environment associated with a case-sensitive
/// key @p key.
char *libhpx_getenv(const char * const key);

#endif // LIBHPX_UTILS_H
