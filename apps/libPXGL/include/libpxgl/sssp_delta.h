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

#ifndef LIBPXGL_SSSP_DELTA_H
#define LIBPXGL_SSSP_DELTA_H

#include <pxgl/pxgl.h>

int _delta_sssp_send_vertex(const hpx_addr_t vertex, const hpx_action_t action, const hpx_addr_t result, const hpx_action_t c_action, const distance_t *const distance, const size_t len);

#endif // LIBPXGL_SSSP_DELTA_H
