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
#ifndef LIBHPX_CONFIG_H
#define LIBHPX_CONFIG_H

/// @file
/// @brief Types and constants needed for configuring HPX at run-time.

//! Configuration options for which global memory model to use.
typedef enum {
  HPX_GAS_DEFAULT = 0, //!< Let HPX choose what memory model to use.
  HPX_GAS_SMP,         //!< Do not use global memory.
  HPX_GAS_PGAS,        //!< Use PGAS (i.e. global memory is fixed).
  HPX_GAS_AGAS,        //!< Use AGAS (i.e. global memory may move).
  HPX_GAS_PGAS_SWITCH, //!< Use hardware-accelerated PGAS.
  HPX_GAS_AGAS_SWITCH, //!< Use hardware-accelerated AGAS.
  HPX_GAS_MAX
} hpx_gas_t;

static const char* const HPX_GAS_TO_STRING[] = {
  "DEFAULT",
  "SMP",
  "PGAS",
  "AGAS",
  "PGAS_SWITCH",
  "AGAS_SWITCH",
  "INVALID_ID"
};


//! Configuration options for the network transports HPX can use.
typedef enum {
  HPX_TRANSPORT_DEFAULT = 0, //!< Let HPX choose what transport to use.
  HPX_TRANSPORT_SMP,         //!< Do not use a network transport.
  HPX_TRANSPORT_MPI,         //!< Use MPI for network transport.
  HPX_TRANSPORT_PORTALS,     //!< Use Portals for network transport.
  HPX_TRANSPORT_PHOTON,      //!< Use Photon for network transport.
  HPX_TRANSPORT_MAX
} hpx_transport_t;

static const char* const HPX_TRANSPORT_TO_STRING[] = {
  "DEFAULT",
  "SMP",
  "MPI",
  "PORTALS",
  "PHOTON",
  "INVALID_ID"
};

typedef enum {
  LIBHPX_NETWORK_DEFAULT = 0,
  LIBHPX_NETWORK_NONE,
  LIBHPX_NETWORK_PHOTON,
  LIBHPX_NETWORK_PORTALS,
  LIBHPX_NETWORK_MPI,
  LIBHPX_NETWORK_MAX
} libhpx_network_t;

static const char * const LIBHPX_NETWORK_TO_STRING[] = {
  "DEFAULT",
  "NONE",
  "PHOTON",
  "PORTALS",
  "MPI",
  "INVALID_ID"
};

//! Configuration options for which bootstrappers HPX can use.
typedef enum {
  HPX_BOOT_DEFAULT = 0,      //!< Let HPX choose what bootstrapper to use.
  HPX_BOOT_SMP,              //!< Use the SMP bootstrapper.
  HPX_BOOT_MPI,              //!< Use mpirun to bootstrap HPX.
  HPX_BOOT_PMI,              //!< Use the PMI bootstrapper.
  HPX_BOOT_MAX
} hpx_boot_t;

static const char* const HPX_BOOT_TO_STRING[] = {
  "DEFAULT",
  "SMP",
  "MPI",
  "PMI",
  ""
};


//! Locality types in HPX.
typedef enum {
  HPX_LOCALITY_NONE = -2,    //!< Represents no locality.
  HPX_LOCALITY_ALL = -1      //!< Represents all localities.
} hpx_locality_t;

static const char* const HPX_LOCALITY_TO_STRING[] = {
  "NONE",
  "ALL"
};


//! Configuration options for runtime logging in HPX.
typedef enum {
  HPX_LOG_DEFAULT = (1<<0),  //!< The default logging level.
  HPX_LOG_BOOT    = (1<<1),  //!< Log the bootstrapper execution.
  HPX_LOG_SCHED   = (1<<2),  //!< Log the HPX scheduler operations.
  HPX_LOG_GAS     = (1<<3),  //!< Log the Global-Address-Space ops.
  HPX_LOG_LCO     = (1<<4),  //!< Log the LCO operations.
  HPX_LOG_NET     = (1<<5),  //!< Turn on logging for network ops.
  HPX_LOG_TRANS   = (1<<6),  //!< Log the transport operations.
  HPX_LOG_PARCEL  = (1<<7),  //!< Parcel logging.
  HPX_LOG_ALL     =   (-1)   //!< Turn on all logging.
} hpx_log_t;

static const char* const HPX_LOG_TO_STRING[] = {
  "LOG_DEFAULT",
  "LOG_BOOT",
  "LOG_SCHED",
  "LOG_GAS",
  "LOG_LCO",
  "LOG_NET",
  "LOG_TRANS",
  "LOG_PARCEL",
  "LOG_ALL"
};


/// The HPX configuration type.
///
/// This configuration is used to control some of the runtime
/// parameters for the HPX system.
typedef struct {
#define LIBHPX_DECL_OPTION(group, type, ctype, id, init) ctype id;
# include "options.def"
#undef LIBHPX_DECL_OPTION
} hpx_config_t;


hpx_config_t *config_new(int *argc, char ***argv)
  HPX_INTERNAL HPX_MALLOC;

void config_delete(hpx_config_t *cfg)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Check to see if the wait flag is set at a particular locality.
///
/// @param     locality The locality to check.
///
/// @returns          0 The flag is not set.
///                   1 The flag is set.
int config_waitat(hpx_config_t *cfg, const hpx_locality_t locality)
  HPX_INTERNAL;

#endif // LIBHPX_CONFIG_H
