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

#ifndef HAVE_JEMALLOC
# error jemalloc support should not be compiled without --enable-jemalloc
#endif

/// @file  libhpx/memory/jemalloc.c
///
/// @brief This file deals with setting up jemalloc when we use it for the
///        system malloc.
#include <jemalloc/jemalloc_local.h>

/// The only thing we currently do is disable jemalloc's ability to madvise() on
/// pages that it thinks that it is no longer using. This prevents jemalloc from
/// dropping translations for pages that we malloc()ed and registered in
/// Photon. This isn't necessary for transports or networks that don't register
/// memory, but we can't set this flag at runtime so we need to conservatively
/// do it here.
const char *malloc_conf = "lg_dirty_mult:-1";
