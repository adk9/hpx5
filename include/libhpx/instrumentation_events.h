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
#ifndef INSTRUMENTATION_EVENTS_H
#define INSTRUMENTATION_EVENTS_H

#include <stddef.h>
#include <stdint.h>
#include <libhpx/instrumentation.h>

// ==================== Event data =============================================

typedef struct record {
  int class;
  int id;
  int rank;
  int worker;
  uint64_t s;
  uint64_t ns;
  uint64_t user[4];
} record_t;

/// The number of columns for a recorded event; some may be unused
#define INST_EVENT_NUM_COLS 10

#define INST_EVENT_COL_OFFSET_CLASS  offsetof(record_t, class)
#define INST_EVENT_COL_OFFSET_ID     offsetof(record_t, id)
#define INST_EVENT_COL_OFFSET_RANK   offsetof(record_t, rank)
#define INST_EVENT_COL_OFFSET_WORKER offsetof(record_t, worker)
#define INST_EVENT_COL_OFFSET_S      offsetof(record_t, s)
#define INST_EVENT_COL_OFFSET_NS     offsetof(record_t, ns)
#define INST_EVENT_COL_OFFSET_USER0  offsetof(record_t, user)
#define INST_EVENT_COL_OFFSET_USER1  offsetof(record_t, user) + 8
#define INST_EVENT_COL_OFFSET_USER2  offsetof(record_t, user) + 16
#define INST_EVENT_COL_OFFSET_USER3  offsetof(record_t, user) + 24

// ==================== Event metadata =========================================

// Header file format (for Abstract Rendering):
//
// Magic file identifier bytes
// table offset
//  [metadata-id, length, data]*
//
// * "table offset" if 4 bytes "metadata-id" is 4 bytes 
// * "length" is 4 bytes, indicates how many bytes to read for the data 
//   portion of this header entry
// * the data is a set of bytes, interpreted in a context-specific manner
// 
// Metadata-id numbers:
// 0 -- types
// 1 -- offsets (a list of ints)
// 2 -- names (pipe separated list of characters)
// 3 -- printf codes (pipe separated list)
// 4 -- min values per column
// 5 -- max values per column
// 
// Type details:
// i: int -- 4 bytes
// l: long -- 8 bytes
// s: short -- 2 bytes
// d: double -- 8 bytes
// f: float -- 4 bytes
// b: byte -- 1 byte
// c: char -- 2 bytes

typedef struct inst_event_col_metadata {
  const char mask; // this should an OR of all the following values:
  const char data_type;      // mask 0x1
  const unsigned int offset; // mask 0x2
  const uint64_t min;        // mask 0x4
  const uint64_t max;        // mask 0x8
  const char printf_code[8]; // mask 0x10 (this value must be nul terminated)
  const char name[256];      // mask 0x20 (this value must be nul terminated)
} inst_event_col_metadata_t;

typedef struct inst_event_metadata {
  const int num_cols;
  // In theory the number of columns need not match the number of fields in
  // an event. In practice, right now they do.
  const  inst_event_col_metadata_t col_metadata[INST_EVENT_NUM_COLS];
} inst_event_metadata_t;

extern const inst_event_metadata_t INST_EVENT_METADATA[HPX_INST_NUM_EVENTS];

/// The following constants are needed for setting the offset value in
/// intances of inst_event_col_metadata_t
// typeof(INST_EVENT_COL_METADATA_CLASS) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_CLASS           \
  { .mask = 0x3f,                               \
      .data_type = 'i',                         \
      .offset = INST_EVENT_COL_OFFSET_CLASS,    \
      .min = 0,                                 \
      .max = HPX_INST_NUM_CLASSES - 1,          \
      .printf_code = "d",                       \
      .name = "class"}

// typeof(INST_EVENT_COL_METADATA_ID) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_ID              \
{ .mask = 0x3f,                                 \
    .data_type = 'i',                           \
    .offset = INST_EVENT_COL_OFFSET_ID,         \
    .min = 0,                                   \
    .max = HPX_INST_NUM_EVENTS,                 \
    .printf_code = "d",                         \
    .name = "event"}

// typeof(INST_EVENT_COL_METADATA_RANK) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_RANK            \
  { .mask = 0x3f,                               \
      .data_type = 'i',                         \
      .offset = INST_EVENT_COL_OFFSET_RANK,     \
      .min = 0,                                 \
      .max = INT_MAX,                           \
      .printf_code = "d",                       \
      .name = "rank"}

// typeof(INST_EVENT_COL_METADATA_WORKER) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_WORKER          \
  { .mask = 0x3f,                               \
      .data_type = 'i',                         \
      .offset = INST_EVENT_COL_OFFSET_WORKER,   \
      .min = 0,                                 \
      .max = INT_MAX,                           \
      .printf_code = "d",                       \
      .name = "worker"}

// typeof(INST_EVENT_COL_METADATA_S) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_S               \
  { .mask = 0x3f,                               \
      .data_type = 'l',                         \
      .offset = INST_EVENT_COL_OFFSET_S,        \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "seconds"}

// typeof(INST_EVENT_COL_METADATA_S) == inst_event_col_metadata_t
// inst_event_col_metadata_t INST_EVENT_COL_METADATA_NS
#define INST_EVENT_COL_METADATA_NS              \
  { .mask = 0x3f,                               \
      .data_type = 'l',                         \
      .offset = INST_EVENT_COL_OFFSET_NS,       \
      .min = 0,                                 \
      .max = 1e9-1,                             \
      .printf_code = "zu",                      \
      .name = "nanoseconds"}

// typeof(INST_EVENT_COL_METADATA_EMPTY0) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_EMPTY0          \
  { .mask = 0x3,                                \
      .data_type = 'l',                         \
      .offset = INST_EVENT_COL_OFFSET_USER0,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = ""                                \
      }

// typeof(INST_EVENT_COL_METADATA_EMPTY1) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_EMPTY1          \
  { .mask = 0x3,                                \
      .data_type = 'l',                         \
      .offset = INST_EVENT_COL_OFFSET_USER1,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = ""                                \
      }

// typeof(INST_EVENT_COL_METADATA_EMPTY2) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_EMPTY2          \
  { .mask = 0x3,                                \
      .data_type = 'l',                         \
      .offset = INST_EVENT_COL_OFFSET_USER2,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = ""                                \
      }

// typeof(INST_EVENT_COL_METADATA_EMPTY3) == inst_event_col_metadata_t
#define INST_EVENT_COL_METADATA_EMPTY3          \
  { .mask = 0x3,                                \
      .data_type = 'l',                         \
      .offset = INST_EVENT_COL_OFFSET_USER3,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = ""                                \
      }

// typeof(INST_EVENT_COL_METADATA_PARCEL_ID) == inst_event_col_metadata_t
#define METADATA_PARCEL_ID                      \
  { .mask = 0x3f,                               \
      .data_type = 'l',                         \
      .offset = INST_EVENT_COL_OFFSET_USER0,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "parcel id"}

// typeof(INST_EVENT_COL_METADATA_PARCEL_ACTION) == inst_event_col_metadata_t
#define METADATA_PARCEL_ACTION                  \
  { .mask = 0x3f,                               \
    .data_type = 'l',                           \
    .offset = INST_EVENT_COL_OFFSET_USER1,      \
    .min = 0,                                   \
    .max = UINT16_MAX,                          \
      .printf_code = "zu",                      \
      .name = "action"}

// typeof(INST_EVENT_COL_METADATA_PARCEL_SIZE) == inst_event_col_metadata_t
#define METADATA_PARCEL_SIZE                    \
  { .mask = 0x3f,                               \
    .data_type = 'l',                           \
    .offset = INST_EVENT_COL_OFFSET_USER2,      \
    .min = 0,                                   \
    .max = UINT32_MAX,                          \
      .printf_code = "zu",                      \
      .name = "parcel size"}

// typeof(INST_EVENT_COL_METADATA_PARCEL_TARGET) == inst_event_col_metadata_t
#define METADATA_PARCEL_TARGET                  \
  { .mask = 0x3f,                               \
    .data_type = 'l',                           \
    .offset = INST_EVENT_COL_OFFSET_USER3,      \
    .min = 0,                                   \
    .max = UINT64_MAX,                          \
      .printf_code = "zu",                      \
      .name = "target address"}

// typeof(INST_EVENT_COL_METADATA_PARCEL_SOURCE) == inst_event_col_metadata_t
#define METADATA_PARCEL_SOURCE                  \
  { .mask = 0x3f,                               \
    .data_type = 'l',                           \
    .offset = INST_EVENT_COL_OFFSET_USER3,      \
    .min = 0,                                   \
    .max = UINT32_MAX,                          \
      .printf_code = "zu",                      \
      .name = "source rank"}

// typeof(METADATA_SCHEDUER_WQ_SIZE) == inst_event_col_metadata_t
#define METADATA_SCHEDULER_WQSIZE               \
  { .mask = 0x3f,                               \
      .data_type = 'l',                         \
      .offset = INST_EVENT_COL_OFFSET_USER0,    \
      .min = 0,                                 \
      .max = UINT64_MAX,                        \
      .printf_code = "zu",                      \
      .name = "work_queue_size"}

// typeof(PARCEL_CREATE_METADATA) == inst_event_metadata_t 
#define PARCEL_CREATE_METADATA                  \
  {                                             \
    .num_cols = 10,                             \
    .col_metadata = {                           \
      INST_EVENT_COL_METADATA_CLASS,            \
      INST_EVENT_COL_METADATA_ID,               \
      INST_EVENT_COL_METADATA_RANK,             \
      INST_EVENT_COL_METADATA_WORKER,           \
      INST_EVENT_COL_METADATA_S,                \
      INST_EVENT_COL_METADATA_NS,               \
      METADATA_PARCEL_ID,                       \
      METADATA_PARCEL_ACTION,                   \
      METADATA_PARCEL_SIZE,                     \
      INST_EVENT_COL_METADATA_EMPTY3            \
    }                                           \
}

// typeof(PARCEL_SEND_METADATA) == inst_event_metadata_t 
#define PARCEL_SEND_METADATA                    \
  {                                             \
    .num_cols = 10,                             \
    .col_metadata = {                           \
      INST_EVENT_COL_METADATA_CLASS,            \
      INST_EVENT_COL_METADATA_ID,               \
      INST_EVENT_COL_METADATA_RANK,             \
      INST_EVENT_COL_METADATA_WORKER,           \
      INST_EVENT_COL_METADATA_S,                \
      INST_EVENT_COL_METADATA_NS,               \
      METADATA_PARCEL_ID,                       \
      METADATA_PARCEL_ACTION,                   \
      METADATA_PARCEL_SIZE,                     \
      METADATA_PARCEL_TARGET                    \
    }                                           \
}

// typeof(PARCEL_RECV_METADATA) == inst_event_metadata_t 
#define PARCEL_RECV_METADATA                    \
  {                                             \
    .num_cols = 10,                             \
    .col_metadata = {                           \
      INST_EVENT_COL_METADATA_CLASS,            \
      INST_EVENT_COL_METADATA_ID,               \
      INST_EVENT_COL_METADATA_RANK,             \
      INST_EVENT_COL_METADATA_WORKER,           \
      INST_EVENT_COL_METADATA_S,                \
      INST_EVENT_COL_METADATA_NS,               \
      METADATA_PARCEL_ID,                       \
      METADATA_PARCEL_ACTION,                   \
      METADATA_PARCEL_SIZE,                     \
      METADATA_PARCEL_SOURCE                    \
    }                                           \
}

// typeof(PARCEL_RUN_METADATA) == inst_event_metadata_t 
#define PARCEL_RUN_METADATA                     \
  {                                             \
  .num_cols = 10,                               \
    .col_metadata = {                           \
      INST_EVENT_COL_METADATA_CLASS,            \
      INST_EVENT_COL_METADATA_ID,               \
      INST_EVENT_COL_METADATA_RANK,             \
      INST_EVENT_COL_METADATA_WORKER,           \
      INST_EVENT_COL_METADATA_S,                \
      INST_EVENT_COL_METADATA_NS,               \
      METADATA_PARCEL_ID,                       \
      METADATA_PARCEL_ACTION,                   \
      METADATA_PARCEL_SIZE,                     \
      INST_EVENT_COL_METADATA_EMPTY3            \
    }                                           \
}

// typeof(PARCEL_END_METADATA) == inst_event_metadata_t 
#define PARCEL_END_METADATA                     \
  {                                             \
  .num_cols = 10,                               \
    .col_metadata = {                           \
      INST_EVENT_COL_METADATA_CLASS,            \
      INST_EVENT_COL_METADATA_ID,               \
      INST_EVENT_COL_METADATA_RANK,             \
      INST_EVENT_COL_METADATA_WORKER,           \
      INST_EVENT_COL_METADATA_S,                \
      INST_EVENT_COL_METADATA_NS,               \
      METADATA_PARCEL_ID,                       \
      METADATA_PARCEL_ACTION,                   \
      INST_EVENT_COL_METADATA_EMPTY2,           \
      INST_EVENT_COL_METADATA_EMPTY3            \
    }                                           \
}

// typeof(PARCEL_END_METADATA) == inst_event_metadata_t 
#define SCHEDULER_WQSIZE_METADATA               \
  {                                             \
  .num_cols = 10,                               \
    .col_metadata = {                           \
      INST_EVENT_COL_METADATA_CLASS,            \
      INST_EVENT_COL_METADATA_ID,               \
      INST_EVENT_COL_METADATA_RANK,             \
      INST_EVENT_COL_METADATA_WORKER,           \
      INST_EVENT_COL_METADATA_S,                \
      INST_EVENT_COL_METADATA_NS,               \
      METADATA_SCHEDULER_WQSIZE,                \
      INST_EVENT_COL_METADATA_EMPTY1,           \
      INST_EVENT_COL_METADATA_EMPTY2,           \
      INST_EVENT_COL_METADATA_EMPTY3            \
    }                                           \
}
#endif
