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

#ifndef METADATA_H
#define METADATA_H

#include <stddef.h>
#include <stdint.h>
#include <libhpx/instrumentation.h>
#include <libhpx/events.h>

// Event data
typedef struct record {
  int worker;
  uint64_t ns;
  uint64_t user[];
} record_t;

// Event metadata
//
// Header file format:
// Magic file identifier bytes =
//   {'h', 'p', 'x', ' ', 'l', 'o', 'g', '\0', 0xFF, 0x00, 0xAA, 0x55}
// table offset
// [metadata-id, length, data]*
//
// * "table offset" is 4 bytes
// Then,
// * "metadata-id" is 4 bytes
// * "length" is 4 bytes, indicates how many bytes to read for the data
//   portion of this header entry
// * the data is a set of bytes, interpreted in a context-specific manner
//
// Metadata-id numbers:
// -1 -- Named value
// 0 -- types
// 1 -- offsets (a list of ints)
// 2 -- names (pipe separated list of characters)
// 3 -- printf codes (pipe separated list)
// 4 -- min values per column
// 5 -- max values per column
//
// The 'data' portion of a named value is [type, value, label].  The 'length' of
// the entry indicates how long this triple is.  Type is a char, value is as
// long as indicated by the type. Label is an string of chars.
//
// Type details:
// i: int    -- 4 bytes
// l: long   -- 8 bytes
// s: short  -- 2 bytes
// d: double -- 8 bytes
// f: float  -- 4 bytes
// b: byte   -- 1 bytes
// c: char   -- 2 bytes

////based on : http://cs-fundamentals.com/tech-interview/c/c-program-to-check-little-and-big-endian-architecture.php
//char endian_flag()
//{
//    unsigned int x = 1;
//    char *c = (char*) &x;
//    return (int)*c == 1 ? '<' : '>';
//}

#define METADATA_TYPE_NAMED_VALUE  -1
#define METADATA_TYPE_DATA_TYPES   0
#define METADATA_TYPE_OFFSETS      1
#define METADATA_TYPE_NAMES        2
#define METADATA_TYPE_PRINTF_CODES 3
#define METADATA_TYPE_MINS         4
#define METADATA_TYPE_MAXS         5

// Type options currently used.  A full list is the keys of numpy.sctypeDict
#define METADATA_TYPE_BYTE   "b"
#define METADATA_TYPE_INT16  "i2"
#define METADATA_TYPE_INT32  "i4"
#define METADATA_TYPE_INT64  "i8"
#define METADATA_TYPE_UINT16 "u2"
#define METADATA_TYPE_UINT32 "u4"
#define METADATA_TYPE_UINT64 "u8"
#define METADATA_TYPE_FLOAT  "f4"
#define METADATA_TYPE_DOUBLE "f8"

//TODO: Verify that type[3] will do null-terminated strings properly
typedef struct inst_named_value {
  const char        type[3];
  const uint32_t   value;
  const char     name[8];
} HPX_PACKED inst_named_value_t;

typedef struct inst_event_col_metadata {
  const char data_type[3];
  const char name[256];
} inst_event_col_metadata_t;

#define METADATA_WORKER                       \
  { .data_type   = METADATA_TYPE_INT32,       \
    .name        = "worker"}

#define METADATA_NS                           \
  { .data_type   = METADATA_TYPE_INT64,       \
    .name        = "nanoseconds"}

#define METADATA_int(_name)            \
  { .data_type   = METADATA_TYPE_INT64,\
    .name        = _name}


//TODO: WHY are all values packaged as int64?  There are other data types...
//Maybe rewrite METADATA_int to _intv(_name, s) {.data_type = METADATA_TYPE_INT##s,...
#define METADATA_uint16_t(_name) METADATA_int(_name)
#define METADATA_uint32_t(_name) METADATA_int(_name)
#define METADATA_uint64_t(_name) METADATA_int(_name)

#define METADATA_size_t METADATA_uint64_t
#define METADATA_hpx_addr_t METADATA_uint64_t
#define METADATA_hpx_action_t METADATA_uint16_t
#define METADATA_uintptr_t METADATA_uint64_t

/// Event metadata struct.
/// In theory the number of columns need not match the number of fields in
/// an event. In practice, right now they do.
typedef struct inst_event_metadata {
  const int num_cols;
  const inst_event_col_metadata_t *col_metadata;
} inst_event_metadata_t;

#define _ENTRY(...) {                                       \
  .num_cols = __HPX_NARGS(__VA_ARGS__)+2,                           \
  .col_metadata =                                                   \
    (const inst_event_col_metadata_t[__HPX_NARGS(__VA_ARGS__)+2]) { \
    METADATA_WORKER,                                                \
    METADATA_NS,                                                    \
    __VA_ARGS__                                                     \
  }                                                                 \
}

static const inst_event_metadata_t INST_EVENT_METADATA[] =
{
# define _MD(t,n) METADATA_##t(_HPX_XSTR(n))
# define _ARGS0() _ENTRY()
# define _ARGS2(t0,n0)                                  \
  _ENTRY(_MD(t0,n0))
# define _ARGS4(t0,n0,t1,n1)                            \
  _ENTRY(_MD(t0,n0),_MD(t1,n1))
# define _ARGS6(t0,n0,t1,n1,t2,n2)                      \
  _ENTRY(_MD(t0,n0),_MD(t1,n1),_MD(t2,n2))
# define _ARGS8(t0,n0,t1,n1,t2,n2,t3,n3)                \
  _ENTRY(_MD(t0,n0),_MD(t1,n1),_MD(t2,n2),        \
         _MD(t3,n3))
# define _ARGS10(t0,n0,t1,n1,t2,n2,t3,n3,t4,n4)         \
  _ENTRY(_MD(t0,n0),_MD(t1,n1),_MD(t2,n2),        \
         _MD(t3,n3),_MD(t4,n4))
# define _ARGSN(...) _HPX_CAT2(_ARGS, __HPX_NARGS(__VA_ARGS__))(__VA_ARGS__)
# define LIBHPX_EVENT(class, event, ...) _ARGSN(__VA_ARGS__),
# include <libhpx/events.def>
#undef LIBHPX_EVENT
# undef _ARGS0
# undef _ARGS2
# undef _ARGS4
# undef _ARGS6
# undef _ARGS8
# undef _ARGS10
# undef _ARGSN
# undef _MD
};

#endif
