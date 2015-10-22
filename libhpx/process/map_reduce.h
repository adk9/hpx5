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
#ifndef LIBHPX_PROCESS_MAP_REDUCE_H
#define LIBHPX_PROCESS_MAP_REDUCE_H

/// Perform a map-reduce operation on an array.
///
/// @param       action The action to run.
/// @param         base The base of the array.
/// @param            n The number of elements in the array.
/// @param       offset The offset within each element to target.
/// @param        bsize The block size for the array.
/// @param          rop The reduction operation.
/// @param        raddr The reduction address.
/// @param        nargs The number of arguments for the action.
/// @param          ... The addresses of each argument.
int _map_reduce(hpx_action_t action, hpx_addr_t base, int n, size_t offset,
                size_t bsize, hpx_action_t rop, hpx_addr_t raddr,int nargs,
                ...);

#define map_reduce(ACTION, BASE, N, OFFSET, BSIZE, ROP, RADDR, ...) \
  _map_reduce(ACTION, BASE, N, OFFSET, BSIZE, ROP, RADDR,           \
              __HPX_NARGS(__VA_ARGS__),##__VA_ARGS__)

#define map(ACTION, BASE, N, OFFSET, BSIZE, ...)                        \
  _map_reduce(ACTION, BASE, N, OFFSET, BSIZE, HPX_ACTION_NULL, HPX_NULL, \
              __HPX_NARGS(__VA_ARGS__),##__VA_ARGS__)

#endif // LIBHPX_PROCESS_MAP_REDUCE_H
