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
#include "address.h"                            /* struct address */

typedef struct header header_t;

/** Forward declarations @{ */
struct hpx_parcel;
/** @} */

/**
 * Represents a serializated parcel.
 */
struct header {
  size_t            size;                       /*!< size of the header */
  unsigned int parcel_id;                       /*!< the parcel idenitifer */
  struct address    dest;                       /*!< HACK! target PA */
  hpx_action_t    action;                       /*!< action key */
  struct hpx_addr target;                       /*!< target address */
  struct hpx_addr   cont;                       /*!< continuation address */
  int              flags;                       /*!< flags */
  size_t    payload_size;                       /*!< sizeof payload */
  uint8_t      payload[];                       /*!< flexible array member */
} HPX_ATTRIBUTE(HPX_ALIGNED(HPX_CACHELINE_SIZE));

/**
 * Serialize a parcel.
 *
 * @param[in]  parcel - the parcel to serialize
 
 * @returns NULL if error, or the serialized parcel (needs to be free-d)
 */
struct header *serialize(const struct hpx_parcel *parcel)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1));

/**
 * Deserialize a parcel.
 *
 * @param[in] header - the header to deserialize
 *
 * @returns the parcel (needs to be free-d), or NULL if there is an error
 */
struct hpx_parcel *deserialize(const struct header *header)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1));

#endif /* LIBHPX_PARCEL_SERIALIZATION_H_ */
