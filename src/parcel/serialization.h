/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_PARCEL_SERIALIZATION_H_
#define LIBHPX_PARCEL_SERIALIZATION_H_

#include "hpx/action.h"                         /* hpx_action_t */
#include "hpx/agas.h"                           /* struct hpx_addr */
#include "hpx/error.h"                          /* hpx_error_t */
#include "hpx/system/attributes.h"              /* HPX_MACROS */

/** Forward declarations @{ */
struct hpx_parcel;
/** @} */

/**
 * Represents a serializated parcel.
 */
struct header {
  unsigned int parcel_id;                       /*!< the parcel idenitifer */
  hpx_action_t    action;                       /*!< action key */
  struct hpx_addr   dest;                       /*!< destination locality */
  int              flags;                       /*!< flags */
  size_t    payload_size;                       /*!< sizeof payload  */
  uint8_t      payload[];                       /*!< flexible array member */
};

/**
 * Serialize a parcel.
 *
 * @param[in]  parcel - the parcel to serialize
 * @param[out] out    - the serialized parcel (needs to be free-d)
 * 
 * @returns HPX_SUCCESS or an error
 */
hpx_error_t serialize(const struct hpx_parcel* parcel, struct header **out)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1, 2));

/**
 * Deserialize a parcel.
 *
 * @param[in]  blob - the header to deserialize
 * @param[out] out  - the parcel (needs to be free-d)
 *
 * @returns HPX_SUCCESS or an error condition
 */
hpx_error_t deserialize(const struct header* blob, struct hpx_parcel** out)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1, 2));

/**
 * Calculate the size of the parcel data.
 *
 * @param[in] blob - the serialized data
 *
 * @returns the size of the payload
 */
size_t get_parcel_size(struct header* blob)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1));

#endif /* LIBHPX_PARCEL_SERIALIZATION_H_ */
