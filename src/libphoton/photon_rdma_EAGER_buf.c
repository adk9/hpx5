#include <stdlib.h>
#include <string.h>

#include "photon_rdma_EAGER_buf.h"
#include "logging.h"
#include "libsync/include/sync.h"

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

  new->size = size;
  new->curr = 0;
  new->tail = 0;

  return new;

error_exit:
  return NULL;
}

void photon_rdma_eager_buf_free(photonEagerBuf buf) {
  free(buf);
}

int photon_rdma_eager_buf_get_offset(photonEagerBuf buf, int size, int lim) {
  uint64_t curr, new, left, tail;
  int offset;

  do {
    curr = sync_load(&buf->curr, SYNC_ACQUIRE);
    tail = sync_load(&buf->tail, SYNC_RELAXED);
    if ((curr - tail) > buf->size) {
      log_err("Exceeded number of outstanding eager buf entries - increase size or wait for completion");
      return -1;
    }
    offset = curr & (buf->size - 1);
    left = buf->size - offset;
    if (left < lim) {
      new = curr + left + size;
      offset = 0;
    }
    else {
      new = curr + size;
    }
  } while (!sync_cas(&buf->curr, curr, new, SYNC_ACQ_REL, SYNC_RELAXED));

  if (left < lim)
    sync_fadd(&buf->tail, left, SYNC_RELAXED);
  
  return offset;
}
