/// @file libhpx/platform/linux/system_barrier.c
/// @brief Wraps pthread barrier with system barrier.

#include <libhpx/system.h>

int system_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
  
  return pthread_barrier_init(barrier, attr, count);
}

int system_barrier_destroy(pthread_barrier_t *barrier) {
  
  return pthread_barrier_destroy(barrier);
}

int system_barrier_wait(pthread_barrier_t *barrier) {
  
  return pthread_barrier_wait(barrier);
}


