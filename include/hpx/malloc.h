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
#ifndef HPX_MALLOC_H
#define HPX_MALLOC_H

#include <stddef.h>

void *malloc(size_t bytes);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *valloc(size_t size);
void *memalign(size_t boundary, size_t size);
int posix_memalign(void **memptr, size_t alignment, size_t size);

#endif
