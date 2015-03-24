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

#include "defs.h"

#include "edge_list.h"
#include "hpx/hpx.h"

// Distance
typedef sssp_uint_t distance_t;

// Vertex Index
typedef sssp_uint_t vertex_t;

extern sssp_uint_t _count_array_block_size;
extern sssp_uint_t _index_array_block_size;

// Graph Edge
typedef struct {
  vertex_t dest;
  distance_t weight;
} adj_list_edge_t;

#define COLOR_GRAY 0 // not visited
#define COLOR_WHITE 1 // visited

// Graph Vertex
typedef struct {
  size_t num_edges;
  distance_t distance;
  unsigned int color;
  adj_list_edge_t edge_list[];
} adj_list_vertex_t;


typedef hpx_addr_t adj_list_t;

// Create an adjacency list from the given edge list. The adjacency
// list includes an index array that points to each row of an
// adjacency list.
extern hpx_action_t adj_list_from_edge_list;
extern int adj_list_from_edge_list_action(const edge_list_t * const el);

extern int reset_adj_list(adj_list_t al, edge_list_t *el);
extern hpx_action_t free_adj_list;
extern int free_adj_list_action(void*);


#endif // PXGL_ADJ_LIST_H
