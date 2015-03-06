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
#ifndef LIBHPX_NETWORK_PWC_PWC_H
#define LIBHPX_NETWORK_PWC_PWC_H

#include <hpx/attributes.h>

/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
struct network;
struct peer;
/// @}

struct network *network_pwc_funneled_new(const struct config *cfg,
                                         struct boot *boot, struct gas *gas)
  HPX_NON_NULL(1,2,3) HPX_MALLOC HPX_INTERNAL;

struct peer *pwc_get_peer(int n)
  HPX_INTERNAL;

#endif
