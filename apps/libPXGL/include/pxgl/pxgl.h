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
#ifndef LIBPXGL_H
#define LIBPXGL_H

// Common definitions
#include "pxgl/defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {_MODE_SSSP, _MODE_BFS } algorithm_mode_t;

// Graph representation / formats
#include "pxgl/adjacency_list.h"
#include "pxgl/edge_list.h"
#include "pxgl/dimacs.h"

// Algorithms
#include "pxgl/sssp.h"
#include "pxgl/bfs.h"

// Metrics
#include "pxgl/gteps.h"
#include "pxgl/statistics.h"

// Termination
#include "pxgl/termination.h"

// Arguments for general PXGL main action
typedef struct {
  char* filename;
  pxgl_uint_t nproblems;
  pxgl_uint_t *problems;
  char *prob_file;
  pxgl_uint_t time_limit;
  int realloc_adj_list;
  termination_t termination;
  graph_generator_type_t graph_generator_type;
  int scale;
  int edgefactor;
  bool checksum;
  algorithm_mode_t mode; // Specifies which algorithm we are running
  void* algorithm_args;  // The algorithm specific arguments
} _pxgl_args_t;


#ifdef __cplusplus
}
#endif

#endif // LIBPXGL_H

