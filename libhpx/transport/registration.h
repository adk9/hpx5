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
#ifndef LIBHPX_TRANSPORT_REGISTRATION_H
#define LIBHPX_TRANSPORT_REGISTRATION_H

#include "hpx/hpx.h"
#include "libhpx/transport.h"

HPX_INTERNAL rkey_t *new_rkey(transport_t *t, char *heap_base)
  HPX_NON_NULL(1);

HPX_INTERNAL rkey_t *exchange_rkey_table(transport_t *t, rkey_t *my_rkey)
  HPX_NON_NULL(1);

#endif // LIBHPX_TRANSPORT_REGISTRATION_H
