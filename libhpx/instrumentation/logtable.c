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

  lseek(fd, size - 1, SEEK_SET);
  if (write(fd, "", 1) != 1) {
    log_error("could not create log file %s\n", filename);
    close(fd);
    return -1;
  }

  return fd;
}

static void *_create_mmap(size_t size, int file) {
  static const int prot = PROT_WRITE;
  static const int flags = MAP_SHARED | MAP_NORESERVE;
  void *base = mmap(NULL, size, prot, flags, file, 0);
  if (base == MAP_FAILED) {
    log_error("could not mmap log file\n");
    base = NULL;
  }
  else {
    log_dflt("mapped %zu byte trace file at %p.\n", size, base);
  }
  return base;
}

int logtable_init(logtable_t *log, const char* filename, size_t size,
                  int class, int id) {
  log->fd = -1;
  log->class = class;
  log->id = id;
  log->record_bytes = 0;
  log->max_size = size;
  log->buffer = NULL;
  sync_store(&log->next, NULL, SYNC_RELEASE);

  if (filename == NULL || size == 0) {
    return LIBHPX_OK;
  }

  log->fd = _create_file(filename, size);
  if (log->fd == -1) {
    goto unwind;
  }

  log->buffer = _create_mmap(size, log->fd);
  if (!log->buffer) {
    goto unwind;
  }

  int fields = TRACE_EVENT_NUM_FIELDS[id];
  log->record_bytes = sizeof(record_t) + fields * sizeof(uint64_t);

  size_t header_size = write_trace_header(log->buffer, class, id);
  assert(((uintptr_t)log->buffer + header_size) % 8 == 0);
  sync_store(&log->next, log->buffer + header_size, SYNC_RELEASE);
  return LIBHPX_OK;

 unwind:
  logtable_fini(log);
  return LIBHPX_ERROR;
}

void logtable_fini(logtable_t *log) {
  if (!log->max_size) {
    return;
  }

  if (log->buffer) {
    if (munmap(log->buffer, log->max_size)) {
      log_error("failed to unmap trace file\n");
    }
  }

  if (log->fd != -1) {
    size_t filesize = sync_load(&log->next, SYNC_ACQUIRE) - log->buffer;
    if (ftruncate(log->fd, filesize)) {
      log_error("failed to truncate trace file\n");
    }
    if (close(log->fd)) {
      log_error("failed to close trace file\n");
    }
  }
}

void logtable_append(logtable_t *log, uint64_t u1, uint64_t u2, uint64_t u3,
                     uint64_t u4) {
}

void logtable_vappend(logtable_t *log, int n, va_list *args) {
  char *next = sync_fadd(&log->next, log->record_bytes, SYNC_ACQ_REL);
  if (next - log->buffer > log->max_size) {
    return;
  }

  record_t *r = (record_t*)next;
  r->worker = HPX_THREAD_ID;
  r->ns = hpx_time_from_start_ns(hpx_time_now());
  for (int i = 0, e = n; i < e; ++i) {
    r->user[i] = va_arg(*args, uint64_t);
  }
}
