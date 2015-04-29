/// @file libhpx/platform/darwin/system_barrier.c
/// @brief Implements pthread_barrier for Darwin (Mac OS X). Credit : http://blog.albertarmea.com/post/47089939939/using-pthread-barrier-on-mac-os-x

#include <libhpx/system.h>
#include <errno.h>

int system_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
  
  if(count == 0) {
    errno = EINVAL;
    return -1;
  }
  if(pthread_mutex_init(&barrier->mutex, 0) < 0) {
    return -1;
  }
  if(pthread_cond_init(&barrier->cond, 0) < 0) {
    pthread_mutex_destroy(&barrier->mutex);
    return -1;
  }
  
  barrier->tripCount = count;
  barrier->count = 0;

  return 0;
}

int system_barrier_destroy(pthread_barrier_t *barrier) {
  
  pthread_cond_destroy(&barrier->cond);
  pthread_mutex_destroy(&barrier->mutex);
  return 0;
}

int system_barrier_wait(pthread_barrier_t *barrier) {

  pthread_mutex_lock(&barrier->mutex);
  ++(barrier->count);
  
  if(barrier->count >= barrier->tripCount) {
    barrier->count = 0;
    pthread_cond_broadcast(&barrier->cond);
    pthread_mutex_unlock(&barrier->mutex);
    return 1;
  }
  else {
    pthread_cond_wait(&barrier->cond, &(barrier->mutex));
    pthread_mutex_unlock(&barrier->mutex);
    return 0;
  }
}


