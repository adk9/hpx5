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
#ifndef HPX_CONFIG_H
#define HPX_CONFIG_H

typedef enum {
  HPX_GAS_DEFAULT = 0,
  HPX_GAS_NOGLOBAL,
  HPX_GAS_PGAS,
  HPX_GAS_AGAS,
  HPX_GAS_PGAS_SWITCH,
  HPX_GAS_AGAS_SWITCH
} hpx_gas_t;

typedef enum {
  HPX_TRANSPORT_DEFAULT = 0,
  HPX_TRANSPORT_SMP,
  HPX_TRANSPORT_MPI,
  HPX_TRANSPORT_PORTALS,
  HPX_TRANSPORT_PHOTON
} hpx_transport_t;

typedef enum {
  HPX_BOOT_DEFAULT = 0,
  HPX_BOOT_SMP,
  HPX_BOOT_MPI,
  HPX_BOOT_PMI
} hpx_boot_t;

typedef enum {
  HPX_WAIT_NONE = 0,
  HPX_WAIT
} hpx_wait_t;

typedef enum {
  HPX_LOCALITY_NONE = -2,
  HPX_LOCALITY_ALL = -1
} hpx_locality_t;

/// ----------------------------------------------------------------------------
/// The HPX configuration type.
/// ----------------------------------------------------------------------------
typedef struct {
  int                 cores;                  // number of cores to run on
  int               threads;                  // number of HPX scheduler threads
  unsigned int  backoff_max;                  // upper bound for backoff
  int           stack_bytes;                  // minimum stack size in bytes
  hpx_gas_t             gas;                  // GAS algorithm
  hpx_transport_t transport;                  // transport to use
  hpx_wait_t           wait;                  // when to wait for a debugger
  hpx_locality_t    wait_at;                  // locality to wait on
  int            statistics;                  // print statistics
} hpx_config_t;

#define HPX_CONFIG_DEFAULTS {                   \
    .cores       = 0,                           \
    .threads     = 0,                           \
    .backoff_max = 1024,                        \
    .stack_bytes = 65536,                       \
    .gas         = HPX_GAS_PGAS,                \
    .transport   = HPX_TRANSPORT_DEFAULT,       \
    .wait        = HPX_WAIT_NONE,               \
    .wait_at     = HPX_LOCALITY_NONE            \
    .statistics  = true                         \
  }

const char* hpx_get_network_id(void);

#endif
