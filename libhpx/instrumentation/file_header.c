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

#include <stdlib.h>
#include <string.h>

#include <hpx/hpx.h>
#include <libhpx/instrumentation_events.h>
#include "file_header.h"

struct cols_metadata {
  int kind;
  int length;
  char metadata[];
};

logtable_header_t LOGTABLE_HEADER = _LOGTABLE_HEADER;

#define METADATA_HANDLER(name, md_kind, ctype)                          \
  static size_t _write_event_metadata_ ## name(void* base,              \
                                  inst_event_metadata_t const *event_md \
                                               ) {                      \
    int el_size = sizeof(ctype);                                        \
    int md_size = sizeof(struct cols_metadata) + event_md->num_cols * el_size; \
    struct cols_metadata *md = malloc(md_size);                         \
    md->kind = (md_kind);                                               \
    md->length = event_md->num_cols * el_size;                          \
    ctype data[event_md->num_cols];                                     \
    for (int i = 0; i < event_md->num_cols; i++) {                      \
      data[i] = event_md->col_metadata[i].name;                         \
    }                                                                   \
    memcpy(md->metadata, data, sizeof(data));                           \
    memcpy(base, md, md_size);                                          \
    free(md);                                                           \
    return md_size;                                                     \
  }

#define METADATA_HANDLER_STR(name, md_kind, _length)                    \
  static size_t _write_event_metadata_ ## name(void* base,              \
                                  inst_event_metadata_t const *event_md \
                                               ) {                      \
    int md_data_size = event_md->num_cols * ((_length) + 1) + 1;        \
    int md_size = sizeof(struct cols_metadata) + md_data_size;          \
    struct cols_metadata *md = malloc(md_size);                         \
    md->kind = (md_kind);                                               \
    char *data = malloc(md_data_size);                                  \
    strncpy(data, event_md->col_metadata[0].name, (_length));           \
    for (int i = 1; i < event_md->num_cols; i++) {                      \
      strncat(data, "|", 1);                                            \
      strncat(data, event_md->col_metadata[i].name, (_length));         \
    }                                                                   \
    strncpy(md->metadata, data, md_data_size);                          \
    md->length = strlen(data);                                          \
    md_size = sizeof(struct cols_metadata) + md->length;                \
    memcpy(base, md, md_size);                                          \
    free(data);                                                         \
    free(md);                                                           \
    return md_size;                                                     \
  }

METADATA_HANDLER(data_type, 0, char)
METADATA_HANDLER(offset, 1, int)
METADATA_HANDLER(min, 4, uint64_t)
METADATA_HANDLER(max, 5, uint64_t)
METADATA_HANDLER_STR(printf_code, 3, 8)
METADATA_HANDLER_STR(name, 2, 256)

/// Write the metadata for the event to the header portion of the log
static size_t _write_event_metadata(void* base, int id) {
  inst_event_metadata_t const *event_md = &INST_EVENT_METADATA[id];
  size_t bytes;
  uintptr_t curr = (uintptr_t)base;
  // 8 byte aligned data first
  bytes =_write_event_metadata_min((void*)curr, event_md);
  curr += bytes;
  bytes =_write_event_metadata_max((void*)curr, event_md);
  curr += bytes;
  // 4 byte-aligned
  bytes =_write_event_metadata_offset((void*)curr, event_md);
  curr += bytes;
  // 1 byte-aligned
  bytes =_write_event_metadata_data_type((void*)curr, event_md);
  curr += bytes;
  bytes =_write_event_metadata_printf_code((void*)curr, event_md);
  curr += bytes;
  bytes =_write_event_metadata_name((void*)curr, event_md);
  curr += bytes;

  return curr - (uintptr_t)base;
}

// Write the metadata for this event to the header of the log file
// Header is padded out to 8 byte multiple length
size_t write_trace_header(void* base, int id) {
  logtable_header_t *header = (logtable_header_t*)base;
  memcpy(header, &LOGTABLE_HEADER, sizeof(LOGTABLE_HEADER));
  size_t metadata_size = _write_event_metadata(header->header_data, id);
  metadata_size += 8 - (metadata_size % 8);
  header->table_offset = offsetof(logtable_header_t, header_data) +
    metadata_size;
  return header->table_offset;
}
