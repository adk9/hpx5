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

#include <hpx/hpx.h>

typedef uint16_t op_t;
typedef uint64_t arg_t;
typedef uint64_t command_t;

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

static inline void command_unpack(command_t cmd, op_t *op, arg_t *arg) {
  *arg = (cmd & ARG_MASK);
  *op = (cmd >> ARG_BITS);
}

#define COMMAND_DEF(type, handler, symbol)                      \
  HPX_ACTION_DEF(type, handler, symbol, HPX_INT, HPX_UINT64)

#define COMMAND_DECL(symbol) HPX_ACTION_DECL(symbol)

// Commands used internally
//
// These are actions (probably interrupts) the take the src and command as
// parameters.
HPX_INTERNAL extern HPX_ACTION_DECL(release_parcel);
HPX_INTERNAL extern HPX_ACTION_DECL(recv_parcel);
HPX_INTERNAL extern HPX_ACTION_DECL(rendezvous_launch);

#endif // LIBHPX_NETWORK_PWC_COMMANDS_H
