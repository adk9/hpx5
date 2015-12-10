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

#ifndef LIBHPX_LIBHPX_ACTIONS_TABLE_H
#define LIBHPX_LIBHPX_ACTIONS_TABLE_H

#include <hpx/hpx.h>

void entry_init_execute_parcel(action_entry_t *entry);
void entry_init_pack_buffer(action_entry_t *entry);
void entry_init_new_parcel(action_entry_t *entry);

#endif // LIBHPX_LIBHPX_ACTIONS_TABLE_H
