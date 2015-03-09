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
#ifndef LIBHPX_INSTRUMENTATION_LOGTABLE_H
#define LIBHPX_INSTRUMENTATION_LOGTABLE_H

#include <hpx/hpx.h>

struct record;

/// The header needed for our data file format
typedef struct {
  const char magic_number[8];
  const uint32_t order;
  uint32_t table_offset;
  char header_data[];
} logtable_header_t;

/// All of the data needed to keep the state of an individual event log
typedef struct {
  hpx_time_t          start;                    // start time
  int                    fd;                    // file backing the log
  int                 class;                    // the class we're logging
  int                    id;                    // the event we're logging
  int                UNUSED;                    // padding
  size_t           max_size;                    // max size in bytes
  logtable_header_t *header;                    // pointer to file header
  struct record    *records;                    // pointer to data for log
  volatile size_t      next;                    // the next element to write
} logtable_t;

#define LOGTABLE_INIT {                         \
    .start = {0},                               \
    .fd = -1,                                   \
    .class = -1,                                \
    .id = -1,                                   \
    .UNUSED = 0,                                \
    .max_size = 0,                              \
    .header = NULL,                             \
    .records = NULL,                            \
    .next = 0                                   \
    }

#define _LOGTABLE_HEADER                                        \
  {                                                             \
    .magic_number = {'h', 'p', 'x', ' ', 'l', 'o', 'g', '\0'},  \
      .order = 0xFF00AA55                                       \
         }

extern logtable_header_t LOGTABLE_HEADER; // == _LOGTABLE_HEADER

/// Initialize a logtable.
///
/// If filename is NULL or size == 0 this will not generate a file.
int logtable_init(logtable_t *lt, const char* filename, size_t size,
                  int class, int event, hpx_time_t start)
    HPX_INTERNAL HPX_NON_NULL(1);

void logtable_fini(logtable_t *lt)
    HPX_INTERNAL HPX_NON_NULL(1);

/// Append a record to a log table.
void logtable_append(logtable_t *log, uint64_t u1, uint64_t u2, uint64_t u3,
                     uint64_t u4)
  HPX_INTERNAL HPX_NON_NULL(1);

#endif // LIBHPX_INSTRUMENTATION_LOGTABLE_H
