#ifndef LOGGING_H
#define LOGGING_H

#include <stdint.h>

#include <hpx/hpx.h>

typedef enum {
  HPX_LOG_CLASS_PARCEL,

  HPX_LOG_NUM_CLASSES
} hpx_logging_class_type_t;

typedef enum {
  HPX_LOG_EVENT_PARCEL_CREATE,
  HPX_LOG_EVENT_PARCEL_SEND,
  HPX_LOG_EVENT_PARCEL_RECV,
  HPX_LOG_EVENT_PARCEL_RUN,
  HPX_LOG_EVENT_PARCEL_END
} hpx_logging_event_type_t;

typedef struct {
  hpx_logging_class_type_t class;      /// event class (i.e. subsystem)
  hpx_logging_event_type_t event_type; /// event symbol
  int priority;                        /// event priority
  hpx_time_t time;                     /// time stamp
  uint64_t id;                         /// event id
  uint64_t data[1];                    /// user data for event
} hpx_logging_event_t; /// represents a logged event in memory

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
  size_t index;       /// current index into log data; must be read/write atomically

} logtable_t;

extern log_t parcel_log;

HPX_INTERNAL int logging_init();
HPX_INTERNAL void logging_fini();

typedef struct {
  int rank;
  size_t bytes;
} hpx_log_event_parcel_create_t;

typedef struct {
  int source;
  int target;
  size_t bytes;
} hpx_log_event_parcel_send_t;

#ifdef HPX_LOG_PARCEL_ENABLED
#define HPX_LOG_EVENT_PARCEL_CREATE((p))                       \
  hpx_log_event_parcel_send_t event;                           \
  event.rank = hpx_get_my_rank();                              \
  event.size = p->size;                                        \
  hpx_logging_log_event(HPX_LOG_CLASS_PARCEL,                  \
                        HPX_LOG_EVENT_PARCEL_CREATE, 0,        \
                        sizeof(hpx_log_event_parcel_create_t)),\
                        event)
#else
#define HPX_LOG_EVENT_PARCEL_CREATE((size))
#endif

#ifdef HPX_LOG_PARCEL_ENABLED
#define HPX_LOG_EVENT_PARCEL_SEND((p))                       \
  hpx_log_event_parcel_send_t event;                         \
  event.source = hpx_get_my_rank();                          \
  event.size = p->size;                                      \
  event.target = p->target;                                  \
  hpx_logging_log_event(HPX_LOG_CLASS_PARCEL,                \
                        HPX_LOG_EVENT_PARCEL_SEND, 0,        \
                        sizeof(hpx_log_event_parcel_send_t), \
                        event)
#else
#define HPX_LOG_EVENT_PARCEL_CREATE((size))
#endif

#ifdef HPX_LOG_PARCEL_ENABLED
#define HPX_LOG_EVENT_PARCEL_RECV((p))                       \
  hpx_log_event_parcel_recv_t event;                         \
  event.source = p->src;                                     \
  event.size = p->size;                                      \
  event.target = p->target;                                  \
  hpx_logging_log_event(HPX_LOG_CLASS_PARCEL,                \
                        HPX_LOG_EVENT_PARCEL_RECV, 0,        \
                        sizeof(hpx_log_event_parcel_recv_t), \
                        event)
#else
#define HPX_LOG_EVENT_PARCEL_CREATE((size))
#endif
