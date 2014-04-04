#ifndef PHOTON_MSGBUFFER_H
#define PHOTON_MSGBUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include "photon_buffer.h"

typedef struct photon_msgbuffer_entry_t {
  void *base;
  void *mptr;
  bool empty;
} photon_mbe;

struct photon_msgbuffer_t {
  photon_mbe *entries;
  int p_count;
  int s_index;
  int status;

  photonBI db;
  uint64_t p_size;
  int p_offset;
  
  pthread_mutex_t buf_lock;
} photon_msgbuf;

typedef struct photon_msgbuffer_t * photonMsgBuf;

photonMsgBuf photon_msgbuffer_new(uint64_t size, uint64_t p_size, int p_offset);
int photon_msgbuffer_free(photonMsgBuf mbuf);

#endif
