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
  const char magic_number[6];
  unsigned char major;
  unsigned char minor;
  uint16_t header_len;
  char header_data[];
} logtable_header_t;

#define _LOGTABLE_HEADER                                       \
  {                                                            \
    .magic_number = {'H', 'P', 'X', 'N', 'P', 'Y'},   \
    .major = 1,                                                \
    .minor = 0,                                                \
  }

typedef struct _cols_metadata {
  int kind;
  int length;
  char metadata[];
} _cols_metadata_t;

static logtable_header_t LOGTABLE_HEADER = _LOGTABLE_HEADER;

//based on : http://cs-fundamentals.com/tech-interview/c/c-program-to-check-little-and-big-endian-architecture.php
const char* endian_flag()
{
    unsigned int x = 1;
    char *c = (char*) &x;
    return (int)*c == 1 ? "<" : ">";
}


static int cat(char *dst, const char *src) {
  //Just like strcat BUT returns number of bytes copied instead of dst.
  strcat(dst, src);
  return strlen(src);
}

static size_t write_header_dict(void* base, const inst_event_metadata_t *event_md, inst_named_value_t* named_values, int num_named_values) {
  const char* endian = endian_flag();

  char *data = (char*) base;
  int written = 0;
  written += cat(data, "{'descr':");
  written += cat(data, "[");

  for (int i=0; i< event_md->num_cols; i++) {
    inst_event_col_metadata_t col = event_md->col_metadata[i];
    written += cat(data, "('");
    written += cat(data, col.name);
    written += cat(data, "', '");
    written += cat(data, endian);
    written += cat(data, col.data_type);
    written += cat(data, "')");
    if (i < event_md->num_cols-1) {
      written += cat(data, ", ");
    }
  }
  written += cat(data, "], ");
  written += cat(data, "'fortran_order': False");

  written += cat(data, ", 'consts': {");
  for (int i=0; i<num_named_values; i++) {
    inst_named_value_t val = named_values[i];
    char val_str[16];
    sprintf(val_str, "%d", val.value);

    written += cat(data, "'");
    written += cat(data, val.name);
    written += cat(data, "': ");
    written += cat(data, val_str);
    if (i < num_named_values-1) {
      written += cat(data, ", ");
    }
  }
  written += cat(data, "}}\n");

  return written;
}


/// Write the metadata for the event to the header portion of the log
static size_t _write_event_metadata(void* base, int class, int id) {
  inst_event_metadata_t const *event_md = &INST_EVENT_METADATA[id];
  uintptr_t curr = (uintptr_t)base;

  // Constants for the header
  inst_named_value_t rank_md = {.value = hpx_get_my_rank(), .name = "rank" };
  inst_named_value_t class_md = {.value = class, .name = "class"};
  inst_named_value_t id_md = {.value = id, .name = "id"};
  inst_named_value_t named_values[] = {rank_md, class_md, id_md};
  
  return write_header_dict((void*) curr, event_md, named_values, 3);
}

// Write the metadata for this event to the header of the log file
// Header is padded out to 16 byte multiple length
size_t write_trace_header(void* base, int class, int id) {
  logtable_header_t *header = (logtable_header_t*)base;
  memcpy(header, &LOGTABLE_HEADER, sizeof(LOGTABLE_HEADER));
  size_t metadata_size = _write_event_metadata(header->header_data, class, id);
  metadata_size += 16 - (metadata_size % 16);
  header->header_len = metadata_size;
  int header_len = offsetof(logtable_header_t, header_data) + metadata_size;
  return header_len;
}
