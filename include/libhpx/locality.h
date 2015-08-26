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

#include <hpx/hpx.h>

#ifdef HAVE_HWLOC
# include <hwloc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Forward declarations.
/// @{
struct action_table;
struct boot;
struct config;
struct network;
struct scheduler;
/// @}

/// The locality object.
///
/// @field      rank The dense, 0-based rank of this locality.
/// @field     ranks The total number of ranks running the current job.
/// @field      boot The bootstrap object. This provides rank and ranks, as well
///                  as some basic, IP-based networking functionality.
/// @field       gas The global address space object. This provides global
///                  memory allocation and address space functionality.
/// @field   network The network layer. This provides an active message
///                  interface targeting global addresses.
/// @field     sched The lightweight thread scheduler. This provides the
///                  infrastructure required to create lightweight threads, and
///                  to deal with inter-thread data and control dependencies
///                  using LCOs.
/// @field    config The libhpx configuration object. This stores the
///                  per-locality configuration parameters based on
///                  the user-specified runtime configuration values
///                  and/or the defaults.
/// @field   actions The symmetric "action table" which stores the
///                  details of all of the actions registered at this locality.
typedef struct locality {
  uint32_t                      rank;
  uint32_t                     ranks;
  struct boot                  *boot;
  void                          *gas;
  struct network            *network;
  struct scheduler            *sched;
  struct config              *config;
  const struct action_table *actions;
#ifdef HAVE_HWLOC
  hwloc_topology_t          topology;
#endif
} locality_t;

/// Inter-locality action interface.
/// @{

/// Used to cause a locality to shutdown.
extern HPX_ACTION_DECL(locality_shutdown);
/// @}

/// The global locality is exposed through this "here" pointer.
extern locality_t *here;

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_LOCALITY_H
