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

#include <libhpx/action.h>

/// The network interface uses a particular action type, the network *command*,
/// which takes an integer indicating the source of the command, and an optional
/// command argument.

/// Command actions should be declared and defined using the following macros.
/// @{
#define COMMAND_DEF(symbol, handler)                                    \
    LIBHPX_ACTION(HPX_INTERRUPT, 0, symbol, handler, HPX_INT, HPX_UINT64)

#define COMMAND_DECL(symbol) HPX_ACTION_DECL(symbol)
/// @}

/// The delete_parcel command will delete a parcel at the target locality.
extern COMMAND_DECL(delete_parcel);

/// The resume_parcel operation will perform parcel_launch() on a parcel at the
/// target locality.
extern COMMAND_DECL(resume_parcel);

/// The resume_parcel operation will perform parcel_launch() on a parcel at the
/// source locality.
extern COMMAND_DECL(resume_parcel_source);

/// This command will set an lco at the target locality.
extern COMMAND_DECL(lco_set);

/// The lco_set_source command will send an lco_set command back to the source
/// locality of a command.
extern COMMAND_DECL(lco_set_source);

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
