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
#ifndef LIBHPX_NETWORK_PWC_COMMANDS_H
#define LIBHPX_NETWORK_PWC_COMMANDS_H

#include <libhpx/network.h>

typedef uint16_t op_t;
typedef uint64_t arg_t;
typedef uint64_t command_t;
typedef int (*command_handler_t)(int, command_t);

#define ARG_BITS (8 * (sizeof(command_t) - sizeof(op_t)))
#define ARG_MASK (UINT64_MAX >> (8 * sizeof(op_t)))

static inline op_t command_get_op(command_t command) {
  return (command >> ARG_BITS);
}

static inline arg_t command_get_arg(command_t command) {
  return (command & ARG_MASK);
}

static inline command_t command_pack(op_t op, arg_t arg) {
  return ((uint64_t)op << ARG_BITS) + (arg & ARG_MASK);
}

#endif // LIBHPX_NETWORK_PWC_COMMANDS_H
