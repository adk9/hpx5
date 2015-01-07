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
#ifndef LIBHPX_NETWORK_PWC_COMPLETIONS_H
#define LIBHPX_NETWORK_PWC_COMPLETIONS_H

#include <hpx/hpx.h>

typedef uint64_t completion_t;

completion_t encode_completion(hpx_action_t op, hpx_addr_t addr);
void decode_completion(completion_t comp, hpx_action_t *op, hpx_addr_t *addr);

#endif // LIBHPX_NETWORK_PWC_COMPLETIONS_H
