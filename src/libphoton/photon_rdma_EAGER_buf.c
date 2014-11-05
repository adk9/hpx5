#include <stdlib.h>
#include <string.h>

#include "photon_rdma_EAGER_buf.h"
#include "logging.h"

photonEagerBuf photon_rdma_eager_buf_create_reuse(uint8_t *eager_buffer, int size) {
  photonEagerBuf new;

  new = malloc(sizeof(struct photon_rdma_eager_buf_t));
  if (!new) {
    log_err("couldn't allocate space for the eager buffer");
    goto error_exit;
  }
  
  memset(new, 0, sizeof(struct photon_rdma_eager_buf_t));
  
  new->data = eager_buffer;
  memset(new->data, 0, sizeof(uint8_t) * size);
  
  new->curr = 0;
  new->ackp = 0;

  return new;

error_exit:
  return NULL;
}

void photon_rdma_eager_buf_free(photonEagerBuf buf) {
  free(buf);
}
