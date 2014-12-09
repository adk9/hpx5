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
#include <hpx/attributes.h>

struct network;
struct gas_class;

struct network *network_pwc_funneled_new(struct gas_class *gas, int nrx)
  HPX_NON_NULL(1) HPX_MALLOC HPX_INTERNAL;

#endif
