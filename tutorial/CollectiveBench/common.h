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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#define TIME() getMicrosecondTimeStamp()
int64_t getMicrosecondTimeStamp();

#ifndef DEFAULT_MAX_MESSAGE_SIZE
#define DEFAULT_MAX_MESSAGE_SIZE (1 << 20)
#endif

#define SKIP 200
#define SKIP_LARGE 10
#define LARGE_MESSAGE_SIZE 8192

#ifndef FIELD_WIDTH
#   define FIELD_WIDTH 20
#endif

#ifndef FLOAT_PRECISION
#   define FLOAT_PRECISION 2
#endif

static int iterations = 1000;
static int iterations_large = 100;
static int print_size = 1;

static void print_header (char *header, int rank, int full) __attribute__((unused));
static void print_data (int rank, int full, int size, double avg_time, double
        min_time, double max_time, int iterations) __attribute__((unused));

typedef struct {
  int index;
  int maxSize;
  hpx_addr_t complete;
  hpx_addr_t collVal;
  hpx_addr_t maxTime;
  hpx_addr_t minTime;
  hpx_addr_t avgTime;
} InitArgs;

typedef struct Domain {
  int index;
  int maxSize;
  hpx_addr_t complete;
  hpx_addr_t collVal;
  hpx_addr_t maxTime;
  hpx_addr_t minTime;
  hpx_addr_t avgTime;
} Domain;
