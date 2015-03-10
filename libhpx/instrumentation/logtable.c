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

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

#include <libsync/sync.h>

#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/instrumentation_events.h>
#include "logtable.h"

logtable_header_t LOGTABLE_HEADER = _LOGTABLE_HEADER;

static size_t _header_size(logtable_t *log) {
  return (size_t)((uintptr_t)log->records - (uintptr_t)log->header);
}

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
  void *base = mmap(NULL, size * sizeof(record_t), prot, flags, file, 0);
  if (base == MAP_FAILED) {
    log_error("could not mmap log file\n");
    return NULL;
  }
  else {
    log("mapped %lu byte trace file at %p.\n", size * sizeof(record_t), base);
  }

  if ((uintptr_t)base % HPX_CACHELINE_SIZE) {
    log("log records are not cacheline aligned\n");
  }

  return base;
}

/// Write the metadata for the event to the header portion of the log
static void *_write_event_metadata(void* base, int id) {
  inst_event_metadata_t event_md = INST_EVENT_METADATA[id];
  memcpy(base, &event_md, sizeof(event_md));
  return (void*)((uintptr_t)base + sizeof(event_md));
}

// Write the metadata for this event to the header of the log file
static void *_write_header(void* base, int id) {
  logtable_header_t *header = (logtable_header_t*)base;
  memcpy(header, &LOGTABLE_HEADER, sizeof(LOGTABLE_HEADER));
  void *new_base = _write_event_metadata(header->header_data, id);
  header->table_offset = (uint32_t)((uintptr_t)new_base - (uintptr_t)base);
  return new_base;
}

int logtable_init(logtable_t *log, const char* filename, size_t size,
                  int class, int id, hpx_time_t start) {
  log->start = start;
  log->fd = -1;
  log->class = class;
  log->id = id;
  sync_store(&log->next, 0, SYNC_RELEASE);
  log->max_size = size;
  log->records = NULL;

  if (filename == NULL || size == 0) {
    return LIBHPX_OK;
  }

  log->fd = _create_file(filename, size);
  if (log->fd == -1) {
    goto unwind;
  }

  log->header = _create_mmap(size, log->fd);
  if (!log->header) {
    goto unwind;
  }

  log->records = _write_header(log->header, id);

  return LIBHPX_OK;

 unwind:
  logtable_fini(log);
  return LIBHPX_ERROR;
}

void logtable_fini(logtable_t *log) {
  if (!log->max_size) {
    return;
  }

  if (log->header) {
    int e = munmap(log->header, log->max_size);
    if (e) {
      log_error("failed to unmap trace file\n");
    }
  }

  if (log->fd != -1) {
    int e = ftruncate(log->fd, log->next * sizeof(record_t));
    if (e) {
      log_error("failed to truncate trace file\n");
    }
    e = close(log->fd);
    if (e) {
      log_error("failed to close trace file\n");
    }
  }
}

void logtable_append(logtable_t *log, uint64_t u1, uint64_t u2, uint64_t u3,
                     uint64_t u4) {
  size_t i = sync_fadd(&log->next, 1, SYNC_ACQ_REL);
  if (_header_size(log) + i * sizeof(record_t) > log->max_size) {
    return;
  }

  record_t *r = &log->records[i];
  r->class = log->class;
  r->id = log->id;
  r->rank = hpx_get_my_rank();
  r->worker = hpx_get_my_thread_id();
  double us = hpx_time_elapsed_us(log->start);
  r->s = us / 1e6;
  r->ns = us * 1e3;
  r->user[0] = u1;
  r->user[1] = u2;
  r->user[2] = u3;
  r->user[3] = u4;
}
