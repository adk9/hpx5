#ifndef PHOTON_RDMA_EAGER_BUF_H
#define PHOTON_RDMA_EAGER_BUF_H

#include <stdint.h>
#include "photon.h"

typedef struct photon_rdma_eager_buf_entry_t {
  uint8_t *data;
} photon_rdma_eager_buf_entry;

typedef struct photon_rdma_eager_buf_t {
  //struct photon_rdma_eager_buf_entry_t *entries;
  uint8_t *data;
  uint64_t curr;
  uint64_t ackp;
  struct photon_buffer_t remote;
} photon_rdma_eager_buf;

typedef struct photon_rdma_eager_buf_entry_t * photonEagerBufEntry;
typedef struct photon_rdma_eager_buf_t       * photonEagerBuf;

PHOTON_INTERNAL photonEagerBuf photon_rdma_eager_buf_create_reuse(uint8_t *eager_buffer, int size);
PHOTON_INTERNAL void photon_rdma_eager_buf_free(photonEagerBuf buf);

#endif
