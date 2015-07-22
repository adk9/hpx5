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

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t op_t;
typedef uint64_t arg_t;

typedef union {
  uint64_t packed;
  struct {
    uint64_t arg : 48;
    uint64_t  op : 16;
  } bits;
} command_t;

typedef int (*command_handler_t)(int, command_t);

static inline op_t command_get_op(command_t command) {
  return command.bits.op;
}

static inline arg_t command_get_arg(command_t command) {
  return command.bits.arg;
}

static inline command_t command_pack(op_t op, arg_t arg) {
  command_t command = {
    .bits = {
      .arg = arg,
      .op = op
    }
  };
  return command;
}

/// Handle a command.
int command_run(int src, command_t cmd);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_NETWORK_PWC_COMMANDS_H
