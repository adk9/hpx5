#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include <stdint.h>
#include <hpx/builtins.h>

struct config;

int inst_init(struct config *cfg)
  HPX_INTERNAL HPX_NON_NULL(1);

void inst_fini(void)
  HPX_INTERNAL;

void inst_vtrace(int class, int id, int n, ...)
  HPX_INTERNAL;

#ifdef ENABLE_INSTRUMENTATION
# define inst_trace(class, id, ...)                             \
  inst_vtrace(class, id, __HPX_NARGS(__VA_ARGS__), __VA_ARGS__)
#else
# define inst_trace(class, id, ...)
#endif

// This matches the order in config.h trace_t.
typedef enum {
  HPX_INST_CLASS_PARCEL = 0,
  HPX_INST_CLASS_NETWORK_PWC,

  HPX_INST_NUM_CLASSES
} hpx_inst_class_type_t;

static const char * const HPX_INST_CLASS_TYPE_TO_STRING[] = {
  "CLASS_PARCEL",
  "CLASS_NETWORK_PWC",
};

typedef enum {
  HPX_INST_EVENT_PARCEL_CREATE = 0,
  HPX_INST_EVENT_PARCEL_SEND,
  HPX_INST_EVENT_PARCEL_RECV,
  HPX_INST_EVENT_PARCEL_RUN,
  HPX_INST_EVENT_PARCEL_END,

  HPX_INST_EVENT_NETWORK_PWC_SEND,
  HPX_INST_EVENT_NETWORK_PWC_RECV,

  HPX_INST_NUM_EVENTS
} hpx_inst_event_type_t;

static const char * const HPX_INST_EVENT_TYPE_TO_STRING[] = {
  "EVENT_PARCEL_CREATE",
  "EVENT_PARCEL_SEND",
  "EVENT_PARCEL_RECV",
  "EVENT_PARCEL_RUN",
  "EVENT_PARCEL_END",
  "EVENT_NETWORK_PWC_SEND",
  "EVENT_NETWORK_PWC_RECV"
};

static const int HPX_INST_CLASS_EVENT_OFFSETS[] = {
  HPX_INST_EVENT_PARCEL_CREATE,
  HPX_INST_EVENT_NETWORK_PWC_SEND,
  HPX_INST_NUM_EVENTS
};

#endif
