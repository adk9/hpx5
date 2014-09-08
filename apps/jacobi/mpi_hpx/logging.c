#include <stdio.h>
#include <string.h>

#if defined(LOGGING_LOCKS) || defined(LOGGING_P2P) || defined(LOGGING_GENERIC)
#define LOGGING_ENABLED
// for logging
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#endif

#include <hpx/hpx.h>
#include <libsync/sync.h>

#include "logging.h"




static struct p2p_log log;
static struct lock_log lock_log;
static struct generic_log generic_log;

static int log_open(const char *name, size_t size, int* fd, void** log_addr) { 
#ifdef LOGGING_ENABLED
  char filename[256];
  snprintf(filename, 255, "./log.%s.%u", name, getpid());
  *fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (*fd == -1) {
    perror("init_action: open");
    return -1;
  }   
  lseek(*fd, P2P_RECORD_LENGTH-1, SEEK_SET);
  write(*fd, "", 1);

  unsigned int record_index = 0;
  *log_addr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED | MAP_NORESERVE, *fd, 0);
  //  printf("mmap at %p\n", *log_addr);
  if (*log_addr == MAP_FAILED) {
    perror("init_action: mmap");
    printf("errno = %d (%s)\n", errno, strerror(errno));
    return -1;
  }   
#endif
  return 0;
}

static void log_close(void* log_addr, size_t size, int fd, size_t fsize){
#ifdef LOGGING_ENABLED
  munmap(log_addr, size);
  int error = ftruncate(fd, fsize);
  if (error)
    perror("shutdown: ftruncate");
  error = close(fd);
  if (error)
    perror("shutdown: close");
#endif
}

int log_p2p_init() {
#ifdef LOGGING_P2P
  int fd;
  int success = log_open("p2p", P2P_RECORD_LENGTH, &log.p2p_record_fd, (void**)&log.p2p_record);
  if (success != 0)
    return success;
  log.p2p_record_index = 0;

  int tid = hpx_thread_get_tls_id();
  hpx_time_t time = hpx_time_now();
  log.p2p_record[0].tid        = tid;
  log.p2p_record[0].start_time = time;
  log.p2p_record[0].end_time   = time;
  log.p2p_record[0].src        = hpx_get_my_rank();
  log.p2p_record[0].dest       = hpx_get_my_rank();
  log.p2p_record[0].action     = E_INIT;
#endif
  return 0;
}

void log_p2p_fini() {
#ifdef LOGGING_P2P
  log_close(log.p2p_record, P2P_RECORD_LENGTH, log.p2p_record_fd, log.p2p_record_index * sizeof(struct p2p_record));
#endif
}

struct p2p_record* log_p2p_start(int action, int src, int dest) {
#ifdef LOGGING_P2P
  unsigned int record_index = sync_fadd(&log.p2p_record_index, 
					1, SYNC_RELAXED);
  int tid = hpx_thread_get_tls_id();
  hpx_time_t time = hpx_time_now();
  log.p2p_record[record_index].tid        = tid;
  log.p2p_record[record_index].start_time = time;
  log.p2p_record[record_index].src        = src;
  log.p2p_record[record_index].dest       = dest;
  log.p2p_record[record_index].action     = action;  
  return &log.p2p_record[record_index];
#else
  return NULL;
#endif
}

void log_p2p_end(struct p2p_record* record) {
#ifdef LOGGING_P2P
  hpx_time_t time = hpx_time_now();
  record->end_time = time;
#endif
}

// for when we don't know the source in advance
void log_p2p_end2(struct p2p_record* record, int src) {
#ifdef LOGGING_P2P
  hpx_time_t time = hpx_time_now();
  record->src = src;
  record->end_time = time;
#endif
}

int log_lock_init() {
#ifdef LOGGING_LOCKS
  int fd;
  int success = log_open("lock", LOCK_RECORD_LENGTH, &lock_log.lock_record_fd, (void**)&lock_log.lock_record);
  if (success != 0)
    return success;
  lock_log.lock_record_index = 0;

  int tid = hpx_thread_get_tls_id();
  hpx_time_t time = hpx_time_now();
  lock_log.lock_record[0].tid          = tid;
  lock_log.lock_record[0].start_time   = time;
  lock_log.lock_record[0].end_time     = time;
  lock_log.lock_record[0].src          = hpx_get_my_rank();
  lock_log.lock_record[0].dest         = hpx_get_my_rank();
  lock_log.lock_record[0].action       = I_INIT;  
#endif
  return 0;
}

struct lock_record* log_lock_start(int action, int src, int dest) {
#ifdef LOGGING_LOCKS
  unsigned int record_index = sync_fadd(&lock_log.lock_record_index, 1, SYNC_RELAXED);
  int tid = hpx_thread_get_tls_id();
  hpx_time_t time = hpx_time_now();
  lock_log.lock_record[record_index].tid    = tid;
  lock_log.lock_record[record_index].start_time   = time;
  lock_log.lock_record[record_index].src    = src;
  lock_log.lock_record[record_index].dest   = dest;
  lock_log.lock_record[record_index].action = action;  
  return &lock_log.lock_record[record_index];
#else
  return NULL;
#endif
}

void log_lock_end(struct lock_record* record) {
#ifdef LOGGING_LOCKS
  hpx_time_t time = hpx_time_now();
  record->end_time = time;
#endif
}

void log_lock_fini() {
#ifdef LOGGING_LOCKS
  log_close(lock_log.lock_record, LOCK_RECORD_LENGTH, lock_log.lock_record_fd, lock_log.lock_record_index * sizeof(struct lock_record));
#endif
}

int log_generic_init() {
#ifdef LOGGING_GENERIC
  int fd;
  int success = log_open("generic", GENERIC_RECORD_LENGTH, &generic_log.generic_record_fd, (void**)&generic_log.generic_record);
  if (success != 0)
    return success;
  generic_log.generic_record_index = 0;

  int tid = hpx_thread_get_tls_id();
  hpx_time_t time = hpx_time_now();
  generic_log.generic_record[0].tid          = tid;
  generic_log.generic_record[0].start_time   = time;
  generic_log.generic_record[0].end_time     = time;
  generic_log.generic_record[0].action       = G_INIT;  
#endif
  return 0;
}

struct generic_record* log_generic_start(int action) {
#ifdef LOGGING_GENERIC
  unsigned int record_index = sync_fadd(&generic_log.generic_record_index, 1, SYNC_RELAXED);
  int tid = hpx_thread_get_tls_id();
  hpx_time_t time = hpx_time_now();
  generic_log.generic_record[record_index].tid    = tid;
  generic_log.generic_record[record_index].start_time   = time;
  generic_log.generic_record[record_index].action = action;  
  return &generic_log.generic_record[record_index];
#else
  return NULL;
#endif
}

void log_generic_end(struct generic_record* record) {
#ifdef LOGGING_GENERIC
  hpx_time_t time = hpx_time_now();
  record->end_time = time;
#endif
}

void log_generic_fini() {
#ifdef LOGGING_GENERIC
  log_close(generic_log.generic_record, GENERIC_RECORD_LENGTH, generic_log.generic_record_fd, generic_log.generic_record_index * sizeof(struct generic_record));
#endif
}

