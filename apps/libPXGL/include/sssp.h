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

typedef struct {
  adj_list_t graph;
  uint64_t source;
} call_sssp_args_t;

// This invokes the chaotic-relaxation SSSP algorithm on the given
// graph, starting from the given source.
extern hpx_action_t call_sssp;
extern int call_sssp_action(call_sssp_args_t *args);

#endif // PXGL_SSSP_H
