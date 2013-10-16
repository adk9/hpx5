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

#include "hpx/error.h"                          /* hpx_error_t */
#include "hpx/action.h"                         /* hpx_action_t */
#include "hpx/agas.h"                           /* hpx_address_t */

/*
 ====================================================================
 Parcel serialization routines.
 ====================================================================
*/


/**
 * Represents a serializated parcel. Should be kept in sync with the
 * struct parcel from hpx/parcel.h"
 */
struct header {
  unsigned int  parcel_id;             /*!< the parcel idenitifer. */
  hpx_action_t  action;                /*!< handle to the associated action. */
  hpx_addr_t    dest;                  /*!< destination locality. */
  int           flags;                 /*!< flags related to the parcel. */
  size_t        payload_size;
  uint8_t       payload[];             /*!< flexible array member */
};

struct hpx_parcel;


/**
   Helper function for sending; combines parcel plus it's payload into
   blob.
  */
hpx_error_t serialize(const struct hpx_parcel* parcel, struct header **out);
hpx_error_t deserialize(const struct header* blob, struct hpx_parcel** out);

#endif /* LIBHPX_PARCEL_SERIALIZATION_H_ */
