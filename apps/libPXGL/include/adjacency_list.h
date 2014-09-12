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

#ifndef PXGL_ADJ_LIST_H
#define PXGL_ADJ_LIST_H

#include "edge_list.h"
#include "hpx/hpx.h"

// Graph Edge
typedef struct {
  uint64_t dest;
  uint64_t weight;
} adj_list_edge_t;


// Graph Vertex
typedef struct {
  volatile uint64_t num_edges;
  volatile uint64_t distance;
  adj_list_edge_t edge_list[];
} adj_list_vertex_t;


typedef hpx_addr_t adj_list_t;

// Create an adjacency list from the given edge list. The adjacency
// list includes an index array that points to each row of an
// adjacency list.
extern hpx_action_t adj_list_from_edge_list;
extern int adj_list_from_edge_list_action(edge_list_t *el);

#endif // PXGL_ADJ_LIST_H
