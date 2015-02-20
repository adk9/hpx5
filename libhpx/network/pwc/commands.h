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
#ifndef LIBHPX_NETWORK_PWC_COMMANDS_H
#define LIBHPX_NETWORK_PWC_COMMANDS_H

#include <hpx/hpx.h>

typedef uint64_t command_t;

hpx_action_t command_get_op(command_t command)
  HPX_INTERNAL;

uint64_t command_get_arg(command_t command)
  HPX_INTERNAL;

command_t encode_command(hpx_action_t op, hpx_addr_t addr)
  HPX_INTERNAL;

void decode_command(command_t cmd, hpx_action_t *op, hpx_addr_t *addr)
  HPX_INTERNAL;

#endif // LIBHPX_NETWORK_PWC_COMMANDS_H
