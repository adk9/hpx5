#ifndef PHOTON_MSGBUFFER_H
#define PHOTON_MSGBUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include "photon_buffer.h"

typedef struct photon_msgbuffer_entry_t {
  void *base;
  void *hptr;
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
  uint64_t m_size;
  int p_offset;
  int p_hsize;
  
  pthread_mutex_t buf_lock;
} photon_msgbuf;

typedef struct photon_msgbuffer_t * photonMsgBuf;

/* 
   @param size:    - total allocated buffer space
   @param p_size   - size of the partitions (includes offset and header)
   @param p_offset - bytes in front of each message
   @param p_hsize  - bytes of header

   .base          .hptr         .mptr
   |...p_offset...|...p_hsize...|......msg......| */
PHOTON_INTERNAL photonMsgBuf photon_msgbuffer_new(uint64_t size, uint64_t p_size, int p_offset, int p_hsize);
PHOTON_INTERNAL int photon_msgbuffer_free(photonMsgBuf mbuf);
PHOTON_INTERNAL photon_mbe *photon_msgbuffer_get_entry(photonMsgBuf mbuf, int *ind);
PHOTON_INTERNAL int photon_msgbuffer_free_entry(photonMsgBuf mbuf, int ind);

extern photonMsgBuf sendbuf;
extern photonMsgBuf recvbuf;

#endif
