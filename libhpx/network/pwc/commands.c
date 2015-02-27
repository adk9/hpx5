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

#include "libhpx/locality.h"
#include "commands.h"
#include "../../gas/pgas/gpa.h"                 // big hack

hpx_action_t command_get_op(command_t command) {
  return (command >> GPA_OFFSET_BITS);
}

uint64_t command_get_arg(command_t command) {
  return (command & GPA_OFFSET_MASK);
}

command_t encode_command(hpx_action_t op, uint64_t arg) {
  dbg_assert(op != HPX_ACTION_NULL || arg == HPX_NULL);
  return ((uint64_t)op << GPA_OFFSET_BITS) + (arg & GPA_OFFSET_MASK);
}

void decode_command(command_t cmd, hpx_action_t *op, hpx_addr_t *addr) {
  *addr = pgas_offset_to_gpa(here->rank, cmd);
  *op = command_get_op(cmd);
}

