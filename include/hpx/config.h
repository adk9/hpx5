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

/// @file
/// @brief Types and constants needed for configuring HPX at run-time.

//! Configuration options for which global memory model to use.
typedef enum {
  HPX_GAS_DEFAULT = 0, //!< Let HPX choose what memory model to use.
  HPX_GAS_NOGLOBAL,    //!< Do not use global memory.
  HPX_GAS_PGAS,        //!< Use PGAS (i.e. global memory is fixed)
  HPX_GAS_AGAS,        //!< Use AGAS (i.e. global memory may move)
  HPX_GAS_PGAS_SWITCH, //!< @todo Document this
  HPX_GAS_AGAS_SWITCH  //!< @todo Document this
} hpx_gas_t;

//! Configuration options for the network transports HPX can use.
typedef enum {
  HPX_TRANSPORT_DEFAULT = 0, //!< Let HPX choose what transport to use.
  HPX_TRANSPORT_SMP,         //!< Do not use a network transport.
  HPX_TRANSPORT_MPI,         //!< Use MPI for network transport.
  HPX_TRANSPORT_PORTALS,     //!< Use Portals for network transport.
  HPX_TRANSPORT_PHOTON       //!< Use Photon for network transport.
} hpx_transport_t;

typedef enum {
  HPX_BOOT_DEFAULT = 0,
  HPX_BOOT_SMP,
  HPX_BOOT_MPI,
  HPX_BOOT_PMI
} hpx_boot_t;

//! Configuration option for whether to wait for a debugger.
typedef enum {
  HPX_WAIT_NONE = 0,  //!< Do not wait
  HPX_WAIT            //!< Wait
} hpx_wait_t;

//! Configuration option for whether to wait for a debugger.
typedef enum {
  HPX_LOCALITY_NONE = -2,  //!< Don't wait after all.
  HPX_LOCALITY_ALL = -1    //!< Wait at all localities.
} hpx_locality_t;

// ----------------------------------------------------------------------------
/// The HPX configuration type (to give hpx_init()).
// ----------------------------------------------------------------------------
/// This configuration can be passed to hpx_init() to control some
/// runtime parameters for the HPX system.

typedef struct {
  int                 cores;                  //!< number of cores to run on
  int               threads;                  //!< number of HPX scheduler threads
  unsigned int  backoff_max;                  //!< upper bound for backoff
  int           stack_bytes;                  //!< minimum stack size in bytes
  hpx_gas_t             gas;                  //!< GAS algorithm
  hpx_transport_t transport;                  //!< transport to use
  hpx_wait_t           wait;                  //!< when to wait for a debugger
  hpx_locality_t    wait_at;                  //!< locality to wait on
  int            statistics;                  //!< print statistics
} hpx_config_t;

/// The default configuration parameters HPX will start with.
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
