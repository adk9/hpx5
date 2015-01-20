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

#include <libhpx/log.h>
#include <config.h>

#include "libhpx/logging.h"
#include "logtable.h"

static size_t logging_max_log_size = 40*1024*1024-1;
static bool hpx_logging_enabled = false;
static logtable_t logtables[HPX_LOG_NUM_CLASSES];
static bool log_class_enable[HPX_LOG_NUM_CLASSES];

static bool get_env_var(char *env_var_name) {
  char* env_var_text;
  env_var_text = getenv(env_var_name);
  if (env_var_text != NULL && atoi(env_var_text))
    return true;
  else
    return false;
}

int log_create(hpx_logging_class_t class, size_t max_size) {
  struct tm lt;
  localtime_r(&t, &lt);  
  char filename[256];
  snprintf(filename, 256, "hpx.%d.%.2d%.2d.%.2d%.2d%.4d.%.3d.log", 
           class, 
           lt.tm_hour, lt.tm_min, lt.tm_mday, lt.tm_mon + 1, 
           lt.tm_year + 1900, 
           hpx_get_my_rank());
  
  int success = logtable_init(&logtable[i], filename, max_size);
  if (success != HPX_ERROR)
    return success;
  return HPX_SUCCESS;
}

int hpx_logging_init() {
#ifdef HPX_LOGGING_ENABLE
  hpx_logging_enabled = get_env_var("HPX_LOGGING_ENABLE");
  if (!hpx_logging_enabled)
    return HPX_SUCCESS;
  
  for (int i = 0; i < HPX_LOG_NUM_CLASSES; i++)
    log_class_enabled[i] = false;

  char* max_log_size_text;
  max_log_size_text = getenv("HPX_LOGGING_MAX_LOG_SIZE");
  if (max_log_size_text != NULL && atoll(max_log_size_text) > 0)
    logging_max_log_size = (size_t)atoll(max_log_size_text);

  size_t max_log_size = getenv
  if (max_log_size > 0)
    logging_max_log_size = max_log_size;

  bool log_parcels = get_env_var("HPX_LOGGING_PARCELS");
  if (log_parcels) {
    log_class_enabled[HPX_LOG_CLASS_PARCEL] = true;
    int success = log_create(class, logging_max_log_size);
    if (success != HPX_SUCCESS)
      return success;
  }

  return HPX_SUCCESS;
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
                           hpx_logging_class_t class,
                           hpx_logging_event_type_t event_type,
                           int priority,
                           int user_data_size,
                           void* user_data
                           ) {
#ifdef HPX_LOGGING_ENABLE
  if (!hpx_logging_enabled)
    return HPX_SUCCESS;
  
  logtable_t lt = logtables[class];
  
  hpx_logging_event_t* record->logtable_next_and_increment(lt);
  record->time = hpx_time_now();
  
  // generate random id? (can't just increment since we're
  // distributed) (also can't use time because small change two events
  // could line up on different ranks)
  //  size_t id = 
  
  // put event in proper table
  hpx_logging_event_t *event = hpx_logtable_append(&logtable[class]);
  
  event->time = hpx_time_now();
  event->class = class;
  event->event_type = event_type;
  // TODO filter based on priority
  event->priority = priority;
  //  event->id = id;

  // where do we determine user_data size? by class, or by event? do we need a different table for each event type not for each class?
  //  event->data = 

#endif
}
