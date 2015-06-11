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
#ifndef LIBHPX_CONFIG_H
#define LIBHPX_CONFIG_H

/// @file
/// @brief Types and constants needed for configuring HPX at run-time.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <hpx/attributes.h>

#ifdef HAVE_PHOTON
# include "config_photon.h"
#endif

#define LIBHPX_OPT_BITSET_ALL UINT64_MAX
#define LIBHPX_OPT_BITSET_NONE 0

//! Configuration options for which global memory model to use.
typedef enum {
  HPX_GAS_DEFAULT = 0, //!< Let HPX choose what memory model to use.
  HPX_GAS_SMP,         //!< Do not use global memory.
  HPX_GAS_PGAS,        //!< Use PGAS (i.e. global memory is fixed).
  HPX_GAS_AGAS,        //!< Use AGAS (i.e. global memory may move).
  HPX_GAS_MAX
} hpx_gas_t;

static const char* const HPX_GAS_TO_STRING[] = {
  "DEFAULT",
  "SMP",
  "PGAS",
  "AGAS",
  "INVALID_ID"
};

//! Configuration options for the network transports HPX can use.
typedef enum {
  HPX_TRANSPORT_DEFAULT = 0, //!< Let HPX choose what transport to use.
  HPX_TRANSPORT_MPI,         //!< Use MPI for network transport.
  HPX_TRANSPORT_PORTALS,     //!< Use Portals for network transport.
  HPX_TRANSPORT_PHOTON,      //!< Use Photon for network transport.
  HPX_TRANSPORT_MAX
} hpx_transport_t;

static const char* const HPX_TRANSPORT_TO_STRING[] = {
  "DEFAULT",
  "MPI",
  "PORTALS",
  "PHOTON",
  "INVALID_ID"
};

typedef enum {
  HPX_NETWORK_DEFAULT = 0,
  HPX_NETWORK_SMP,
  HPX_NETWORK_PWC,
  HPX_NETWORK_ISIR,
  HPX_NETWORK_MAX
} libhpx_network_t;

static const char * const HPX_NETWORK_TO_STRING[] = {
  "DEFAULT",
  "SMP",
  "PWC",
  "ISIR",
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
  "INVALID_ID"
};

//! Locality types in HPX.
#define HPX_LOCALITY_NONE  -2                   //!< Represents no locality.
#define HPX_LOCALITY_ALL   -1                   //!< Represents all localities.

//! Configuration options for runtime logging in HPX.
#define HPX_LOG_DEFAULT 1               //!< The default logging level.
#define HPX_LOG_BOOT    2               //!< Log the bootstrapper execution.
#define HPX_LOG_SCHED   4               //!< Log the HPX scheduler operations.
#define HPX_LOG_GAS     8               //!< Log the Global-Address-Space ops.
#define HPX_LOG_LCO     16              //!< Log the LCO operations.
#define HPX_LOG_NET     32              //!< Turn on logging for network ops.
#define HPX_LOG_TRANS   64              //!< Log the transport operations.
#define HPX_LOG_PARCEL  128             //!< Parcel logging.
#define HPX_LOG_ACTION  256             //!< Log action registration.
#define HPX_LOG_CONFIG  512             //!< Log configuration.
#define HPX_LOG_MEMORY  1024            //!< Log memory (coarse grained)

static const char *const HPX_LOG_LEVEL_TO_STRING[] = {
  "default",
  "boot",
  "sched",
  "gas",
  "lco",
  "net",
  "trans",
  "parcel",
  "action",
  "config",
  "memory"
};

#define HPX_TRACE_PARCELS 1
#define HPX_TRACE_PWC     2
#define HPX_TRACE_SCHED   3

static const char *const HPX_TRACE_CLASS_TO_STRING[] = {
  "parcels",
  "pwc",
  "sched",
  "lco",
  "process"
};

/// The HPX configuration type.
///
/// This configuration is used to control some of the runtime
/// parameters for the HPX system.
typedef struct config {
#define LIBHPX_OPT(group, id, init, ctype) ctype group##id;
# include "options.def"
#undef LIBHPX_OPT
} config_t;

config_t *config_new(int *argc, char ***argv)
  HPX_MALLOC;

void config_delete(config_t *cfg);

void config_print(config_t *cfg, FILE *file);

/// Add declarations to query each of the set options.
///
/// @param          cfg The configuration to query.
/// @param        value The value to check for.
#define LIBHPX_OPT_INTSET(group, id, UNUSED2, UNUSED3, UNUSED4)     \
  int config_##group##id##_isset(const config_t *cfg, int value)    \
    HPX_NON_NULL(1);

#define LIBHPX_OPT_BITSET(group, id, UNUSED2)                       \
  static inline uint64_t                                            \
  config_##group##id##_isset(const config_t *cfg, uint64_t mask) {  \
    return (cfg->group##id & mask);                                 \
  }
# include "options.def"
#undef LIBHPX_OPT_BITSET
#undef LIBHPX_OPT_INTSET

#endif // LIBHPX_CONFIG_H
