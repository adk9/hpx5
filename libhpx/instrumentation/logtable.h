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
#ifndef LOGTABLE_H
#define LOGTABLE_H

/// Forward declarations.
/// @{
struct hpx_inst_event;
struct logtable;
/// @}

unsigned int get_logging_record_size(unsigned int user_data_size);

int logtable_init(struct logtable *lt, const char* filename, size_t total_size);

void logtable_fini(struct logtable *lt);

struct hpx_inst_event *logtable_next_and_increment(struct logtable *lt);

#endif
