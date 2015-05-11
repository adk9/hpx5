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
  #include <hwloc.h>
#endif 

#include <libhpx/debug.h>
#include <libhpx/gas.h>

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
typedef struct locality {
  uint32_t                      rank;
  uint32_t                     ranks;
  struct boot                  *boot;
  struct gas                    *gas;
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

typedef struct {
  hpx_action_t action;
  hpx_status_t status;
  char data[];
} locality_cont_args_t;

extern hpx_action_t locality_call_continuation;
/// @}

/// The global locality is exposed through this "here" pointer.
extern locality_t *here;

#endif // LIBHPX_LOCALITY_H
