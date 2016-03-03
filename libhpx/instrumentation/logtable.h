// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

struct record;

/// @struct logtable_t
/// @brief
/// All of the data needed to keep the state of an individual event log
/// @var logtable_t::fd
/// file backing the log
/// @var logtable_t::class
/// the class we're logging
/// @var logtable_t::id
/// the event we're logging
/// @var logtable_t::UNUSED
/// padding
/// @var logtable_t::max_size
/// max size in bytes
/// @var logtable_t::header
/// pointer to file header
/// @var logtable_t::records
/// pointer to data for log
/// @var logtable_t::next
/// the next element to write
/// @var logtable_t::last
/// the last element written
typedef struct {
  int                 fd;       
  int              class;       
  int                 id;       
  int             UNUSED;       
  size_t        max_size;       
  void           *header;       
  struct record *records;       
  volatile size_t   next;       
  volatile size_t   last;       
} logtable_t;

#define LOGTABLE_INIT {          \
  .fd       = -1,                \
  .class    = -1,                \
  .id       = -1,                \
  .UNUSED   = 0,                 \
  .max_size = 0,                 \
  .header   = NULL,              \
  .records  = NULL,              \
  .next     = 0,                 \
  .last     = 0                  \
}

/// Initialize a logtable.
///
/// If filename is NULL or size == 0 this will not generate a file.
int logtable_init(logtable_t *lt, const char* filename, size_t size,
                  int class, int event);

void logtable_fini(logtable_t *lt);

/// Append a record to a log table.
void logtable_append(logtable_t *log, uint64_t u1, uint64_t u2, uint64_t u3,
                     uint64_t u4);

#endif // LIBHPX_INSTRUMENTATION_LOGTABLE_H
