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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

#include <libsync/sync.h>

#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include "metadata.h"
#include "file_header.h"
#include "logtable.h"

static int _create_file(const char *filename, size_t size) {
  static const int flags = O_RDWR | O_CREAT;
  static const int perm = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int fd = open(filename, flags, perm);
  if (fd == -1) {
    log_error("failed to open a log file for %s\n", filename);
    return -1;
  }

  return fd;
}

void logtable_init(logtable_t *log, const char* filename, size_t size,
                   int class, int id) {
  log->fd = -1;
  log->id = id;
  log->record_bytes = 0;
  log->max_size = size;
  log->buffer = NULL;

  if (filename == NULL || size == 0) {
    return;
  }

  log->fd = _create_file(filename, size);
  if (log->fd == -1) {
    log_error("could not create log file %s\n", filename);
    return;
  }

  log->buffer = malloc(size);
  if (!log->buffer) {
    log_error("problem allocating buffer for %s\n", filename);
    close(log->fd);
    return;
  }
  log->next = log->buffer;

  int fields = TRACE_EVENT_NUM_FIELDS[id];
  log->record_bytes = sizeof(record_t) + fields * sizeof(uint64_t);

  char *buffer = malloc(32768);
  log->header_size = write_trace_header(buffer, class, id);
  if (write(log->fd, buffer, log->header_size) != log->header_size) {
    log_error("failed to write header to file\n");
  }
  free(buffer);
}

void logtable_fini(logtable_t *log) {
  if (!log->max_size) {
    return;
  }

  if (log->fd != -1) {
    size_t filesize = log->next - log->buffer;
    write(log->fd, log->buffer, filesize);
    if (close(log->fd)) {
      log_error("failed to close trace file\n");
    }
  }
}

void logtable_vappend(logtable_t *log, int n, va_list *args) {
  uint64_t time = hpx_time_from_start_ns(hpx_time_now());
  char *next = log->next + log->record_bytes;
  if (next - log->buffer > log->max_size) {
    EVENT_FILE_IO_BEGIN();
    write(log->fd, log->buffer, log->next - log->buffer);
    EVENT_FILE_IO_END();
    next = log->buffer;
  }
  log->next = next;

  record_t *r = (record_t*)next;
  r->worker = self->id;
  r->ns = time;
  for (int i = 0, e = n; i < e; ++i) {
    r->user[i] = va_arg(*args, uint64_t);
  }
}
