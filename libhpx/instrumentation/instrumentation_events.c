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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>
#include <stdint.h>

#include <libhpx/instrumentation.h>
#include <libhpx/instrumentation_events.h>

// Possibly, we might want to move more macros from the header into here as
// variables

const inst_event_metadata_t INST_EVENT_METADATA[HPX_INST_NUM_EVENTS] =
{
  PARCEL_CREATE_METADATA,
  PARCEL_SEND_METADATA,
  PARCEL_RECV_METADATA,
  PARCEL_RUN_METADATA,
  PARCEL_END_METADATA,
  PARCEL_SUSPEND_METADATA,
  PARCEL_RESUME_METADATA,
  {0},
  {0},
  SCHEDULER_WQSIZE_METADATA,
  LCO_INIT_METADATA,
  LCO_DELETE_METADATA,
  LCO_RESET_METADATA,
  LCO_WAIT_METADATA,
  LCO_TRIGGER_METADATA
};
