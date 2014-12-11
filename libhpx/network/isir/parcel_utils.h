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
#ifndef LIBHPX_NETWORK_ISIR_PARCEL_UTILS_H
#define LIBHPX_NETWORK_ISIR_PARCEL_UTILS_H

#include <mpi.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>

static inline int payload_size_to_mpi_bytes(uint32_t payload) {
  return payload + sizeof(hpx_parcel_t) - offsetof(hpx_parcel_t, action);
}

static inline uint32_t mpi_bytes_to_payload_size(int bytes) {
  assert(bytes > 0);
  return bytes + offsetof(hpx_parcel_t, action) - sizeof(hpx_parcel_t);
}

static inline int payload_size_to_tag(uint32_t payload) {
  uint32_t parcel_size = payload + sizeof(hpx_parcel_t);
  int tag = ceil_div_32(parcel_size, HPX_CACHELINE_SIZE);
  DEBUG_IF(true) {
    int tag_ub;
    int flag;
    int e = MPI_Comm_get_attr(MPI_COMM_WORLD, MPI_TAG_UB, &tag_ub, &flag);
    dbg_check(e, "Could not extract tag upper bound\n");
    if (tag_ub <= tag) {
      dbg_error("tag value out of bounds (%d > %d)\n", tag, tag_ub);
    }
  }
  return tag;
}

static inline uint32_t tag_to_payload_size(int tag) {
  uint32_t parcel_size = tag * HPX_CACHELINE_SIZE;
  return parcel_size - sizeof(hpx_parcel_t);
}

static inline uint32_t tag_to_mpi_bytes(int tag) {
  uint32_t parcel_size = tag * HPX_CACHELINE_SIZE;
  return parcel_size - offsetof(hpx_parcel_t, action);
}


#endif // LIBHPX_NETWORK_ISIR_PARCEL_UTILS_H
