#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include <stdint.h>

#include <hpx/hpx.h>

#ifdef ENABLE_INSTRUMENTATION
#define HPX_INST_PARCEL_ENABLED // for now, enabled by default
#endif

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

static const int HPX_INST_CLASS_EVENT_OFFSET[] = {
  0,
  5,
  7
};

static const int HPX_INST_EVENTS_PER_CLASS[] = {5, 0};
static const int HPX_INST_FIRST_EVENT_FOR_CLASS[] = {0, 5};

typedef struct hpx_inst_event {
  hpx_inst_class_type_t class;      /// event class (i.e. subsystem)
  hpx_inst_event_type_t event_type; /// event symbol
  int rank;
  int worker;
  int thread;
  //  int priority;                 /// event priority
  uint64_t s;
  uint64_t ns;
  //  hpx_time_t time;              /// time stamp
  //  uint64_t id;                  /// event id
  uint64_t data[4];                 /// user data for event
} hpx_inst_event_t; /// represents a logged event in memory

typedef struct logtable {

  // parameters of log
  int record_size;    /// size in bytes per record
  char filename[256]; /// log name to be used for log filename
  size_t data_size;   /// the maximum size of the log's raw data

  // at-initialization data
  bool inited;        /// has it been initialized?
  void *data;         /// pointer to data for log
  int fd;             /// file descriptor log is being written to

  // variable data
  size_t index;       /// index into log data; must be read/write atomically

} logtable_t;

HPX_INTERNAL int hpx_inst_init();
HPX_INTERNAL void hpx_inst_fini();
HPX_INTERNAL void hpx_inst_log_event(hpx_inst_class_type_t class,
                                     hpx_inst_event_type_t event_type,
                                     int priority,
                                     int user_data_size,
                                     void* user_data);

typedef struct {
  uint64_t id;
  uint64_t action;
  uint64_t size;
} hpx_inst_event_parcel_create_t;

typedef struct {
  uint64_t id;
  uint64_t action;
  uint64_t size;
  uint64_t target;
} hpx_inst_event_parcel_send_t;

typedef struct {
  uint64_t id;
  uint64_t action;
  uint64_t size;
  uint64_t source;
} hpx_inst_event_parcel_recv_t;

typedef struct {
  uint64_t id;
  uint64_t action;
  uint64_t size;
} hpx_inst_event_parcel_run_t;

typedef struct {
  uint64_t id;
  uint64_t action;
  uint64_t size;
} hpx_inst_event_parcel_end_t;

typedef struct {
  uint64_t sequence;
  uint64_t bytes;
  uint64_t address;
  uint64_t target_rank;
} hpx_inst_event_network_pwc_send_t;

typedef struct {
  uint64_t sequence;
  uint64_t bytes;
  uint64_t address;
  uint64_t source_rank;
} hpx_inst_event_network_pwc_recv_t;

extern bool hpx_inst_enabled;
extern bool hpx_inst_parcel_enabled;

#ifdef HPX_INST_PARCEL_ENABLED
#define HPX_INST_EVENT_PARCEL_CREATE(p)                         \
  if (hpx_inst_enabled && hpx_inst_parcel_enabled) {            \
    hpx_inst_event_parcel_create_t event;                       \
    event.id = (p)->id;                                         \
    event.action = (p)->action;                                 \
    event.size = (p)->size;                                     \
    hpx_inst_log_event(HPX_INST_CLASS_PARCEL,                  \
                        HPX_INST_EVENT_PARCEL_CREATE, 0,        \
                        sizeof(hpx_inst_event_parcel_create_t), \
                        &event);                                \
  }
#else
#define HPX_INST_EVENT_PARCEL_CREATE(p)
#endif

#ifdef HPX_INST_PARCEL_ENABLED
#define HPX_INST_EVENT_PARCEL_SEND(p)                           \
  if (hpx_inst_enabled && hpx_inst_parcel_enabled) {            \
    hpx_inst_event_parcel_send_t event;                         \
    event.id = (p)->id;                                         \
    event.action = (p)->action;                                 \
    event.size = (p)->size;                                     \
    event.target = (p)->target;                                 \
    hpx_inst_log_event(HPX_INST_CLASS_PARCEL,                  \
                        HPX_INST_EVENT_PARCEL_SEND, 0,          \
                        sizeof(hpx_inst_event_parcel_send_t),   \
                        &event);                                \
  }
#else
#define HPX_INST_EVENT_PARCEL_SEND(p)
#endif

#ifdef HPX_INST_PARCEL_ENABLED
#define HPX_INST_EVENT_PARCEL_RECV(p)                           \
  if (hpx_inst_enabled && hpx_inst_parcel_enabled) {            \
    hpx_inst_event_parcel_recv_t event;                         \
    event.id = (p)->id;                                         \
    event.action = (p)->action;                                 \
    event.size = (p)->size;                                     \
    event.source = (p)->src;                                    \
    hpx_inst_log_event(HPX_INST_CLASS_PARCEL,                  \
                        HPX_INST_EVENT_PARCEL_RECV, 0,          \
                        sizeof(hpx_inst_event_parcel_recv_t),   \
                        &event);                                \
  }
#else
#define HPX_INST_EVENT_PARCEL_RECV(p)
#endif

#ifdef HPX_INST_PARCEL_ENABLED
#define HPX_INST_EVENT_PARCEL_RUN(p)                            \
  if (hpx_inst_enabled && hpx_inst_parcel_enabled) {            \
    hpx_inst_event_parcel_run_t event;                          \
    event.id = (p)->id;                                         \
    event.action = (p)->action;                                 \
    event.size = (p)->size;                                     \
    hpx_inst_log_event(HPX_INST_CLASS_PARCEL,                  \
                        HPX_INST_EVENT_PARCEL_RUN, 0,           \
                        sizeof(hpx_inst_event_parcel_run_t),    \
                        &event);                                \
  }
#else
#define HPX_INST_EVENT_PARCEL_RUN(p)
#endif

#ifdef HPX_INST_PARCEL_ENABLED
#define HPX_INST_EVENT_PARCEL_END(p)                            \
  if (hpx_inst_enabled && hpx_inst_parcel_enabled) {            \
    hpx_inst_event_parcel_end_t event;                          \
    event.id = (p)->id;                                         \
    event.action = (p)->action;                                 \
    event.size = (p)->size;                                     \
    hpx_inst_log_event(HPX_INST_CLASS_PARCEL,                  \
                        HPX_INST_EVENT_PARCEL_END, 0,           \
                        sizeof(hpx_inst_event_parcel_end_t),    \
                        &event);                                \
  }
#else
#define HPX_INST_EVENT_PARCEL_END(p)
#endif

#ifdef ENABLE_INSTRUMENTATION
#define HPX_INST_INIT hpx_inst_init
#define HPX_INST_FINI hpx_inst_fini
#else
#define HPX_INST_INIT()
#define HPX_INST_FINI()
#endif

#endif
