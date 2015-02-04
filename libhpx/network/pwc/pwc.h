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
#ifndef LIBHPX_NETWORK_PWC_PWC_H
#define LIBHPX_NETWORK_PWC_PWC_H


#include <hpx/hpx.h>


/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
struct network;
struct peer;
/// @}


struct network *network_pwc_funneled_new(struct config *, struct boot *,
                                         struct gas *, int nrx)
  HPX_NON_NULL(1, 2) HPX_MALLOC HPX_INTERNAL;

struct peer *pwc_get_peer(struct network *pwc, int src)
  HPX_INTERNAL;

#endif
