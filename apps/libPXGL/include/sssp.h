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

#ifndef PXGL_SSSP_H
#define PXGL_SSSP_H
//#define GATHER_STAT 1

typedef struct {
  uint64_t useful_work;
  uint64_t useless_work;
  uint64_t edge_traversal_count;
}_sssp_statistics;

typedef struct {
  adj_list_t graph;
  uint64_t source;
#ifdef GATHER_STAT
  hpx_addr_t sssp_stat;
#endif
  hpx_addr_t termination_lco;
} call_sssp_args_t;

typedef enum { COUNT_TERMINATION, AND_LCO_TERMINATION } termination_t;
extern termination_t termination;

// This invokes the chaotic-relaxation SSSP algorithm on the given
// graph, starting from the given source.
extern hpx_action_t call_sssp;
extern int call_sssp_action(const call_sssp_args_t *const args);

#endif // PXGL_SSSP_H
