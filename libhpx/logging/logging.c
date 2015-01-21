#include <inttypes.h> // PRIuXX
#include <stdio.h>
#include <stdlib.h> // atoi and getenv
#include <string.h>
#include <time.h> // time()

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

//#include <dlfcn.h> // dlsym

#include <hpx/hpx.h>
//#include <libhpx/network.h> // network_class_t
#include <libhpx/parcel.h>
#include <libsync/sync.h>

#include <config.h>

#include "libhpx/logging.h"
#include "logtable.h"

#ifdef HPX_LOG_ENABLED
static logtable_t logtables[HPX_LOG_NUM_EVENTS];
static size_t logging_max_log_size = 40*1024*1024-1;
static bool hpx_logging_enabled = false;
static bool log_class_enabled[HPX_LOG_NUM_CLASSES];
static char log_dir_name[256];
static hpx_time_t time_start;
static bool logging_active = false; // not whether we WANT to log, but whether 
                                    // we are ABLE to (i.e. after initialization
                                    // and before shutdown)

static bool get_env_var(char *env_var_name) {
  char* env_var_text;
  env_var_text = getenv(env_var_name);
  if (env_var_text != NULL && atoi(env_var_text))
    return true;
  else
    return false;
}

static void 
time_diff(uint64_t *out_s, uint64_t *out_ns, hpx_time_t *start, hpx_time_t *end) {
  {
    if ((end->tv_nsec-start->tv_nsec)<0) {
      *out_s = end->tv_sec-start->tv_sec-1;
      *out_ns = (1e9+end->tv_nsec)-start->tv_nsec;
    } else {
      *out_s = end->tv_sec-start->tv_sec;
      *out_ns = end->tv_nsec-start->tv_nsec;
    }
  }
}

static int 
log_create(hpx_logging_class_type_t class, hpx_logging_event_type_t event, 
               size_t max_size) {
  char filename[256];
  snprintf(filename, 256, "log.%d.%d.%d.log", 
           class, event, hpx_get_my_rank());
  
  int success = logtable_init(&logtables[event], filename, max_size);
  if (success != HPX_ERROR)
    return success;
  return HPX_SUCCESS;
}
#endif

int hpx_logging_init() {
#ifdef HPX_LOG_ENABLED
  int success = -1;

  hpx_logging_enabled = get_env_var("HPX_LOGGING_ENABLE");
  if (!hpx_logging_enabled)
    return HPX_SUCCESS;
  
  for (int i = 0; i < HPX_LOG_NUM_CLASSES; i++)
    log_class_enabled[i] = false;

  char* max_log_size_text;
  max_log_size_text = getenv("HPX_LOGGING_MAX_LOG_SIZE");
  if (max_log_size_text != NULL && atoll(max_log_size_text) > 0)
    logging_max_log_size = (size_t)atoll(max_log_size_text);

  // change to user-specified root directory
  char* log_rootdir;
  log_rootdir = getenv("HPX_LOGGING_ROOTDIR");
  if (log_rootdir != NULL) {
    success = chdir(log_rootdir);
    if (success != 0) {
      perror("Specified root directory for logging not found.");
      return -1;
    }
  }

  // create directory for log files
  time_t t = time(NULL);
  struct tm lt;
  localtime_r(&t, &lt);  
  snprintf(log_dir_name, 256, "hpx.%.2d%.2d.%.2d%.2d%.4d.%.3d", 
           lt.tm_hour, lt.tm_min, lt.tm_mday, lt.tm_mon + 1, 
           lt.tm_year + 1900, 
           hpx_get_my_rank());
  success = mkdir(log_dir_name, 0777);
  if (success == 0)
    success = chdir(log_dir_name);
  if (success != 0) {
    perror("Could not create output directory for logging.");
    return -1;
  }

  // create log files
  bool log_parcels = get_env_var("HPX_LOGGING_PARCELS");
  if (log_parcels) {
    log_class_enabled[HPX_LOG_CLASS_PARCEL] = true;
    int class = HPX_LOG_CLASS_PARCEL;
    for (int i = 0; i < HPX_LOG_EVENTS_PER_CLASS[class]; i++) {
      int event = HPX_LOG_FIRST_EVENT_FOR_CLASS[class] + i;
      int success = log_create(class, event, logging_max_log_size);

      // TODO real error handling
      if (success != HPX_SUCCESS)
        return success;
    }
  }

  time_start = hpx_time_now();

  logging_active = true;
#endif
  return HPX_SUCCESS;
}

void hpx_logging_fini() {
#ifdef HPX_LOG_ENABLED
  logging_active = false;
  for (int i = 0; i < HPX_LOG_NUM_EVENTS; i++)
    if (logtables[i].inited == true)
      logtable_fini(&logtables[i]);
#endif
}

/// Record an event to the log
/// @param class          Class this event is part of (see 
///                       hpx_logging_class_type_t)
/// @param event_type     The type of this event (see hpx_logging_event_type_t)
/// @param priority       The priority of this event (may be filtered out if
///                       below some threshhold)
/// @param user_data_size The size of the data to record
/// @param user_data      The data to record (is copied)
void hpx_logging_log_event(
                           hpx_logging_class_type_t class,
                           hpx_logging_event_type_t event_type,
                           int priority,
                           int user_data_size,
                           void* user_data
                           ) {
#ifdef HPX_LOG_ENABLED
  if (!hpx_logging_enabled)
    return;
  
  if (!logging_active)
    return;

  logtable_t *lt = &logtables[event_type];
  
  hpx_logging_event_t* event = logtable_next_and_increment(lt);
  if (event == NULL)
    return;

  event->class = class;
  event->event_type = event_type;
  event->rank = hpx_get_my_rank();
  event->worker = hpx_get_my_thread_id();
  //  event->thread = hpx_thread_get_tls_id();
  hpx_time_t time_now = hpx_time_now();
  time_diff(&event->s, &event->ns, &time_start, &time_now);

  //  event->priority = priority;

  // generate random id? (can't just increment since we're
  // distributed) (also can't use time because small change two events
  // could line up on different ranks)
  //  event->id = id;

  memcpy(event->data, user_data, user_data_size);

#endif
}

