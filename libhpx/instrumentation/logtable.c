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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <time.h>                       /// @todo: use platform independent code
#include <unistd.h>
#include <sys/mman.h>

#include <libsync/sync.h>

#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "logtable.h"

typedef struct record {
  int class;
  int id;
  int rank;
  int worker;
  uint64_t s;
  uint64_t ns;
  uint64_t user[4];
} record_t;

static void _time_diff(record_t *r, hpx_time_t *start) {
  hpx_time_t end = hpx_time_now();
  if (end.tv_nsec < start->tv_nsec) {
    r->s = end.tv_sec - start->tv_sec - 1;
    r->ns = (1e9 + end.tv_nsec) - start->tv_nsec;
  } else {
    r->s = end.tv_sec - start->tv_sec;
    r->ns = end.tv_nsec - start->tv_nsec;
  }
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

int logtable_init(logtable_t *log, const char* filename, size_t size,
                  int class, int id, hpx_time_t start) {
  log->start = start;
  log->fd = -1;
  log->class = class;
  log->id = id;
  sync_store(&log->next, 0, SYNC_RELEASE);
  log->size = size;
  log->records = NULL;

  if (filename == NULL || size == 0) {
    return LIBHPX_OK;
  }

  log->fd = _create_file(filename, size);
  if (log->fd == -1) {
    goto unwind;
  }

  log->records = _create_mmap(size, log->fd);
  if (!log->records) {
    goto unwind;
  }

  return LIBHPX_OK;

 unwind:
  logtable_fini(log);
  return LIBHPX_ERROR;
}

void logtable_fini(logtable_t *log) {
  if (!log->size) {
    return;
  }

  if (log->records) {
    int e = munmap(log->records, log->size * sizeof(record_t));
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
  if (i > log->size) {
    return;
  }

  record_t *r = &log->records[i];
  r->class = log->class;
  r->id = log->id;
  r->rank = hpx_get_my_rank();
  r->worker = hpx_get_my_thread_id();
  _time_diff(r, &log->start);
  r->user[0] = u1;
  r->user[1] = u2;
  r->user[2] = u3;
  r->user[3] = u4;
}
