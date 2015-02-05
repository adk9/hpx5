#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include <stdint.h>

#include <hpx/hpx.h>

typedef enum {
  HPX_INST_CLASS_PARCEL,

  HPX_INST_NUM_CLASSES
} hpx_inst_class_type_t;

typedef enum {
  HPX_INST_EVENT_PARCEL_CREATE,
  HPX_INST_EVENT_PARCEL_SEND,
  HPX_INST_EVENT_PARCEL_RECV,
  HPX_INST_EVENT_PARCEL_RUN,
  HPX_INST_EVENT_PARCEL_END,

  HPX_INST_NUM_EVENTS
} hpx_inst_event_type_t;

static const int HPX_INST_EVENTS_PER_CLASS[] = {5, 0};
static const int HPX_INST_FIRST_EVENT_FOR_CLASS[] = {0, 5};

typedef struct {
  hpx_inst_class_type_t class;      /// event class (i.e. subsystem)
  hpx_inst_event_type_t event_type; /// event symbol
  int rank;
  int worker;
  int thread;
  //  int priority;                    /// event priority
  uint64_t s;
  uint64_t ns;
  //  hpx_time_t time;                 /// time stamp
  //  uint64_t id;                     /// event id
  uint64_t data[4];                    /// user data for event
} hpx_inst_event_t; /// represents a logged event in memory

typedef struct {

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

static const int HPX_INST_SIZEOF_EVENT[] = {
  sizeof(hpx_inst_event_parcel_create_t),
  sizeof(hpx_inst_event_parcel_send_t),
  sizeof(hpx_inst_event_parcel_recv_t),
  sizeof(hpx_inst_event_parcel_run_t),
  sizeof(hpx_inst_event_parcel_end_t)
};

#ifdef HPX_INST_PARCEL_ENABLED
#define HPX_INST_EVENT_PARCEL_CREATE(p)                         \
  hpx_inst_event_parcel_create_t event;                         \
  event.id = (p)->id;                                           \
  event.action = (p)->action;                                   \
  event.size = (p)->size;                                       \
  hpx_inst_inst_event(HPX_INST_CLASS_PARCEL,                    \
                      HPX_INST_EVENT_PARCEL_CREATE, 0,          \
                      sizeof(hpx_inst_event_parcel_create_t),   \
                      &event)
#else
#define HPX_INST_EVENT_PARCEL_CREATE(p)
#endif

#ifdef HPX_INST_PARCEL_ENABLED
#define HPX_INST_EVENT_PARCEL_SEND(p)                           \
  hpx_inst_event_parcel_send_t event;                           \
  event.id = (p)->id;                                           \
  event.action = (p)->action;                                   \
  event.size = (p)->size;                                       \
  event.target = (p)->target;                                   \
  hpx_inst_inst_event(HPX_INST_CLASS_PARCEL,                    \
                      HPX_INST_EVENT_PARCEL_SEND, 0,            \
                      sizeof(hpx_inst_event_parcel_send_t),     \
                      &event)
#else
#define HPX_INST_EVENT_PARCEL_SEND(p)
#endif

#ifdef HPX_INST_PARCEL_ENABLED
#define HPX_INST_EVENT_PARCEL_RECV(p)                           \
  hpx_inst_event_parcel_recv_t event;                           \
  event.id = (p)->id;                                           \
  event.action = (p)->action;                                   \
  event.size = (p)->size;                                       \
  event.source = (p)->src;                                      \
  hpx_inst_inst_event(HPX_INST_CLASS_PARCEL,                    \
                      HPX_INST_EVENT_PARCEL_RECV, 0,            \
                      sizeof(hpx_inst_event_parcel_recv_t),     \
                      &event)
#else
#define HPX_INST_EVENT_PARCEL_RECV(p)
#endif

#ifdef HPX_INST_PARCEL_ENABLED
#define HPX_INST_EVENT_PARCEL_RUN(p)                            \
  hpx_inst_event_parcel_run_t event;                            \
  event.id = (p)->id;                                           \
  event.action = (p)->action;                                   \
  event.size = (p)->size;                                       \
  hpx_inst_inst_event(HPX_INST_CLASS_PARCEL,                    \
                      HPX_INST_EVENT_PARCEL_RUN, 0,             \
                      sizeof(hpx_inst_event_parcel_run_t),      \
                      &event)
#else
#define HPX_INST_EVENT_PARCEL_RUN(p)
#endif

#ifdef HPX_INST_PARCEL_ENABLED
#define HPX_INST_EVENT_PARCEL_END(p)                            \
  hpx_inst_event_parcel_end_t event;                            \
  event.id = (p)->id;                                           \
  event.action = (p)->action;                                   \
  event.size = (p)->size;                                       \
  hpx_inst_inst_event(HPX_INST_CLASS_PARCEL,                    \
                      HPX_INST_EVENT_PARCEL_END, 0,             \
                      sizeof(hpx_inst_event_parcel_end_t),      \
                      &event)
#else
#define HPX_INST_EVENT_PARCEL_END(p)
#endif

#ifdef HPX_INST_ENABLED
#define HPX_INST_INIT hpx_inst_init
#define HPX_INST_FINI hpx_inst_fini
#else
#define HPX_INST_INIT()
#define HPX_INST_FINI()
#endif

#endif
