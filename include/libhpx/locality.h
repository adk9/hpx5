// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifndef LIBHPX_LOCALITY_H
#define LIBHPX_LOCALITY_H

/// @file include/libhpx/locality.h
/// @brief Exports all of the resources available at an HPX locality.
///
/// The locality is modeled as a structure containing the basic ingredients of
/// the LIBHPX runtime present in each PE. It is exposed within the library
/// through the global "here" object that gets initialized during hpx_init().
///
/// In addition to this object, a small set of actions is available for inter-PE
/// AM-based communication.
///
/// Furthermore, this file defines a number of inline convenience functions that
/// wrap common functionality that needs access to the global here object.

#include <signal.h>
#include <hpx/hpx.h>

#ifdef __cplusplus
namespace libhpx {
class GAS;
class Network;
class Scheduler;
}
using libhpx::GAS;
using libhpx::Network;
using libhpx::Scheduler;
extern "C" {
#else
#define GAS void
#define Network void
#define Scheduler void
#endif

/// Forward declarations.
/// @{
struct boot;
struct config;
struct topology;
struct tracer;
/// @}

/// The locality object.
typedef struct locality {
  uint32_t               rank; //!< The dense, 0-based rank of this locality.
  uint32_t              ranks; //!< The total number of ranks running the
                               //!< current job.
  struct boot           *boot; //!< The bootstrap object. This provides rank
                               //!< and ranks, as well as some basic, IP-based
                               //!< networking functionality.
  GAS                    *gas; //!< The global address space object. This
                               //!< provides global memory allocation and
                               //!< address space functionality.
  Network                *net; //!< The network layer. This provides an active
                               //!< message interface targeting global
                               //!< addresses.
  Scheduler            *sched; //!< The lightweight thread scheduler. This
                               //!< provides the infrastructure required to
                               //!< create lightweight threads, and to deal
                               //!< with inter-thread data and control
                               //!< dependencies using LCOs.
  struct config       *config; //!< The libhpx configuration object. This
                               //!< stores the per-locality configuration
                               //!< parameters based on the user-specified
                               //!< runtime configuration values and/or the
                               //!< defaults.
  struct topology   *topology; //!< The topology information.
  void           *percolation; //!< An interface for dealing with GPU backends.
  struct trace        *tracer; //!< Reference to the tracer object
  sigset_t               mask; //!< The default signal mask.
} locality_t;

/// The global locality is exposed through this "here" pointer.
///
/// @todo: Why is this symbol exposed as public? It's not exposed in libhpx.h,
///        which is the only libhpx installed header...
extern locality_t *here HPX_PUBLIC;

#ifdef __cplusplus
}
#else
# undef Scheduler
#endif

#endif // LIBHPX_LOCALITY_H
