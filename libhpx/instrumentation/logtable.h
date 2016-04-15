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

/// All of the data needed to keep the state of an individual event log
typedef struct logtable {
  int               fd;                         //!< file backing the log
  int               id;                         //!< the event we're logging
  int     record_bytes;                         //!< record size
  char         *buffer;                         //!< pointer to buffer
  char * volatile next;                         //!< pointer to next record
  size_t      max_size;                         //!< max size in bytes
  size_t   header_size;                         //!< header size in bytes
} logtable_t;

/// Initialize a logtable.
///
/// This will create a file-backed mmaped buffer for this event for logging
/// purposes.
///
/// @param           lt The log table to initialize.
/// @param     filename The name of the file to create and map.
/// @param         size The number of bytes to allocate.
/// @param        class The event class.
/// @param        event The event type.
void logtable_init(logtable_t *lt, const char* filename, size_t size,
                  int class, int event);

/// Clean up a log table.
///
/// It is safe to call this on an uninitialized log table. If the log table was
/// initialized this will unmap its buffer, and truncate and close the
/// corresponding file.
///
/// @param           lt The log table to finalize.
void logtable_fini(logtable_t *lt);

/// Append a record to a log table.
///
/// Log tables have variable length record sizes based on their event type. This
/// allows the user to log a variable number of features with each trace point.
///
/// @param           lt The log table.
/// @param            n The number of features to log.
/// @param         args The features to log.
void logtable_vappend(logtable_t *lt, int n, va_list *args);

#endif // LIBHPX_INSTRUMENTATION_LOGTABLE_H
