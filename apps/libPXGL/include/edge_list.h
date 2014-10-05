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

#ifndef PXGL_EDGE_LIST_H
#define PXGL_EDGE_LIST_H

#include "hpx/hpx.h"

typedef struct {
  uint64_t source;
  uint64_t dest;
  uint64_t weight;
} edge_list_edge_t;


typedef struct {
  uint64_t       num_edges;
  uint64_t    num_vertices;
  uint64_t min_edge_weight;
  uint64_t max_edge_weight;
  hpx_addr_t     edge_list;
  uint32_t edge_list_bsize;
} edge_list_t;


// This action "returns" (continues) the constructed edge list as a
// edge_list_t structure.
extern hpx_action_t edge_list_from_file;
extern int edge_list_from_file_action(char **filename);

#endif // PXGL_EDGE_LIST_H
