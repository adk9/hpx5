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

#include <stdlib.h>
#include <string.h>

#include <hpx/hpx.h>
#include "metadata.h"
#include "file.h"

/// The header needed for our data file format
typedef struct {
  const char magic_number[7];
  unsigned char major;
  unsigned char minor;
  uint32_t header_len;
  char header_data[];
} logtable_header_t;

#define _LOGTABLE_HEADER                                       \
  {                                                            \
    .magic_number = {'\x93', 'N', 'U', 'M', 'P', 'Y', '\0'},   \
    .major = 1,                                                \
    .minor = 0,                                                \
  }

typedef struct _cols_metadata {
  int kind;
  int length;
  char metadata[];
} _cols_metadata_t;

static logtable_header_t LOGTABLE_HEADER = _LOGTABLE_HEADER;

static size_t
_write_event_metadata_named_value(void* base, inst_named_value_t const *nv_md)
{
  // nv_md = [type, value, label] (e.g. [METADATA_TYPE_INT32, 4, "rank"]
  // metadata = [type, length, nv_md] where
  //   type = METADATA_TYPE_NAMED_VALUE and 
  //   length = sizeof(metadata) + sizeof(nv_md)
  _cols_metadata_t *md = (_cols_metadata_t*)base;
  md->kind = METADATA_TYPE_NAMED_VALUE;
  md->length = sizeof(inst_named_value_t);
  memcpy(md->metadata, nv_md, md->length);
  return sizeof(*md) + md->length;
}

#define METADATA_HANDLER(name, md_kind, ctype)                          \
  static size_t _write_event_metadata_ ## name(void* base,              \
    inst_event_metadata_t const *event_md) {                            \
    if (event_md->num_cols == 0) {                                      \
      return 0;                                                         \
    }                                                                   \
    _cols_metadata_t *md = (_cols_metadata_t*)base;                     \
    md->kind = (md_kind);                                               \
    md->length = event_md->num_cols * sizeof(ctype);                    \
    ctype *data = (ctype*)md->metadata;                                 \
    for (int i = 0; i < event_md->num_cols; i++) {                      \
      data[i] = event_md->col_metadata[i].name;                         \
    }                                                                   \
    return sizeof(*md) + md->length;                                    \
  }

#define METADATA_HANDLER_STR(name, md_kind, _length)                    \
  static size_t _write_event_metadata_ ## name(void* base,              \
    inst_event_metadata_t const *event_md) {                            \
    if (event_md->num_cols == 0) {                                      \
      return 0;                                                         \
    }                                                                   \
    _cols_metadata_t *md = (_cols_metadata_t*)base;                     \
    md->kind = (md_kind);                                               \
    int md_data_size = event_md->num_cols * ((_length) + 1) + 1;        \
    char *data = (char*)md->metadata;                                   \
    strncpy(data, event_md->col_metadata[0].name, (_length));           \
    for (int i = 1; i < event_md->num_cols; i++) {                      \
      strncat(data, "|", 1);                                            \
      strncat(data, event_md->col_metadata[i].name, (_length));         \
    }                                                                   \
    md->length = md_data_size;                                          \
    return sizeof(*md) + md_data_size;                                  \
  }

METADATA_HANDLER(data_type, METADATA_TYPE_DATA_TYPES, char)
METADATA_HANDLER_STR(name, METADATA_TYPE_NAMES, 256)

static size_t write_numpy_dict(void* base, const inst_event_metadata_t *event_md) {
  char *data = (char*) base;                                   \
  int written = 0;
  strncat(data, "{'desc': [", 9);
  written += 9;

  for (int i=0; i< event_md->num_cols; i++) {
    inst_event_col_metadata_t col = event_md->col_metadata[i];
    strncat(data, "('", 2);
    strncat(data, col.name, strlen(col.name));
    strncat(data, "':'", 3);
    strncat(data, col.data_type, strlen(col.data_type));
    strncat(data, "')", 2);
    written += 2 + strlen(col.name) + 3 + strlen(col.data_type) + 2;
    if (i < event_md->num_cols-1) {
      strncat(data, ",", 1);
      written += 1;
    }
  }
  strncat(data, "]", 1);
  written += 1;

  strncat(data, ", 'fortran_order': false", 24);
  written += 24;

  strncat(data, ", 'shape': (          ,)", 24);
  written +=24;
   
  return written;
}

/// Write the metadata for the event to the header portion of the log
static size_t _write_event_metadata(void* base, int class, int id) {
  inst_event_metadata_t const *event_md = &INST_EVENT_METADATA[id];
  uintptr_t curr = (uintptr_t)base;
  // 1 byte-aligned
  curr += write_numpy_dict((void*) curr, event_md);
  // 1 byte-aligned named values
  // record rank
  inst_named_value_t rank_md = {
    .type = METADATA_TYPE_INT32,
    .value = hpx_get_my_rank(),
    .name = "rank"
  };
  curr += _write_event_metadata_named_value((void*)curr, &rank_md);
  // event class
  inst_named_value_t class_md = {.type = METADATA_TYPE_INT32, .value = class,
                                .name = "class"};
  curr += _write_event_metadata_named_value((void*)curr, &class_md);
  inst_named_value_t id_md = {.type = METADATA_TYPE_INT32,
                              .value = id, .name = "id"};
  curr += _write_event_metadata_named_value((void*)curr, &id_md);
  return curr - (uintptr_t)base;
}

// Write the metadata for this event to the header of the log file
// Header is padded out to 16 byte multiple length
size_t write_trace_header(void* base, int class, int id) {
  logtable_header_t *header = (logtable_header_t*)base;
  memcpy(header, &LOGTABLE_HEADER, sizeof(LOGTABLE_HEADER));
  size_t metadata_size = _write_event_metadata(header->header_data, class, id);
  metadata_size += 16 - (metadata_size % 16);
  header->header_len = offsetof(logtable_header_t, header_data) +
    metadata_size;
  return header->header_len;
}
