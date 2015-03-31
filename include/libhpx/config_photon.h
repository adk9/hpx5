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
#ifndef LIBHPX_CONFIG_PHOTON_H
#define LIBHPX_CONFIG_PHOTON_H

//! Configuration options for the network transports HPX can use.
typedef enum {
  HPX_PHOTON_BACKEND_DEFAULT = 0, //!< Set a default (verbs).
  HPX_PHOTON_BACKEND_VERBS,       //!< Use Verbs photon backend.
  HPX_PHOTON_BACKEND_UGNI,        //!< Use uGNI photon backend.
  HPX_PHOTON_BACKEND_MAX
} hpx_photon_backend_t;

static const char* const HPX_PHOTON_BACKEND_TO_STRING[] = {
  "default",
  "verbs",
  "ugni",
  "INVALID_BACKEND"
};

#endif
