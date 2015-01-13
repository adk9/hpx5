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

#ifndef LIBPXGL_SSSP_COMMON_H
#define LIBPXGL_SSSP_COMMON_H

#include <pxgl/pxgl.h>

bool _try_update_vertex_distance(adj_list_vertex_t *const vertex, distance_t distance);
void _send_update_to_neighbors(adj_list_vertex_t *const vertex, distance_t distance);
extern hpx_action_t _sssp_visit_vertex;
extern sssp_kind_t _sssp_kind;
extern adj_list_t graph;
extern hpx_action_t _sssp_initialize_graph;

#endif // LIBPXGL_SSSP_COMMON_H
