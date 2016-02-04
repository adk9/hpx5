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

#ifndef LIBHPX_DATA_COALESCED_COALESCED_H
#define LIBHPX_DATA_COALESCED_COALESCED_H

#include <stdarg.h>
#include <libhpx/action.h>
#include <libhpx/gas.h>

int coalesce_data(hpx_addr_t addr, hpx_action_t id,  uint64_t dest,
		  va_list *args, int n);

void flush (action_t *action);
extern hpx_action_t _demux;
#endif // LIBHPX_DATA_COALESCED_COALESCED_H
