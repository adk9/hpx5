#ifndef LOGGING_H
#define LOGGING_H

#ifdef  __cplusplus
#include <stdint.h>
#if defined(__linux__)
#include <time.h>
typedef struct timespec hpx_time_t;
#define HPX_TIME_INIT {0}
#elif defined(__APPLE__)
#include <stdint.h>
typedef uint64_t hpx_time_t;
#define HPX_TIME_INIT (0)
#endif
#else // if  __cplusplus
#include <hpx/hpx.h>
#endif


// =================================================================

struct generic_record {
  int tid;
  int action;
  hpx_time_t start_time;
  hpx_time_t end_time;
};

enum {G_INIT, G_DURATION};

static const uint64_t GENERIC_RECORD_LENGTH = 1024*1024*10 * sizeof(struct generic_record);

struct generic_log {
  unsigned int generic_record_index;
  struct generic_record *generic_record;
  int generic_record_fd;
};

// =================================================================

struct lock_record {
  int tid;
  int action;
  int src;
  int dest;
  hpx_time_t start_time;
  hpx_time_t end_time;
};

enum {I_INIT, COMM_P, COMM_V, P2P_P, P2P_V};

static const uint64_t LOCK_RECORD_LENGTH = 1024*1024*100 * sizeof(struct lock_record);

struct lock_log {
  unsigned int lock_record_index;
  struct lock_record *lock_record;
  int lock_record_fd;
};

// =================================================================

enum {E_INIT, E_ISEND, E_SEND, E_IRECV, E_RECV, E_WAIT, E_WAITALL};

struct p2p_record {
  int tid;
  int action;
  int src;
  int dest;
  hpx_time_t start_time;
  hpx_time_t end_time;
};

struct p2p_log {
  unsigned int p2p_record_index;
  struct p2p_record *p2p_record;
  int p2p_record_fd;
};


static const uint64_t P2P_RECORD_LENGTH = 1024*1024*100 * sizeof(struct p2p_record);

int log_p2p_init();
void log_p2p_fini();
struct p2p_record* log_p2p_start(int action, int src, int dest);
void log_p2p_end(struct p2p_record* record);
void log_p2p_src(struct p2p_record* record, int src);

int log_lock_init();
void log_lock_fini();
struct lock_record* log_lock_start(int action, int src, int dest);
void log_lock_end(struct lock_record* record);

int log_generic_init();
struct generic_record* log_generic_start(int action);
void log_generic_end(struct generic_record* record);
void log_generic_fini();

#endif
