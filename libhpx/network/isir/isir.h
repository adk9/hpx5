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
#ifndef LIBHPX_NETWORK_ISIR_ISIR_H
#define LIBHPX_NETWORK_ISIR_ISIR_H

#include <hpx/hpx.h>
#include <hpx/attributes.h>


/// Forward declarations.
/// @{
struct gas;
struct network;
/// @}


/// Allocate a new Isend/Irecv funneled network.
struct network *network_isir_funneled_new(struct gas *gas, int nrx)
  HPX_NON_NULL(1) HPX_MALLOC HPX_INTERNAL;


#endif
