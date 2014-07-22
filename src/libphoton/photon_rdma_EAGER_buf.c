#include <stdlib.h>
#include <string.h>

#include "photon_rdma_EAGER_buf.h"
#include "logging.h"

photonEagerBuf photon_rdma_eager_buf_create_reuse(photonEagerBufEntry eager_buffer, int size) {
  photonEagerBuf new;

  new = malloc(sizeof(struct photon_rdma_eager_buf_t));
  if (!new) {
    log_err("couldn't allocate space for the eager buffer");
    goto error_exit;
  }

  memset(new, 0, sizeof(struct photon_rdma_eager_buf_t));

  new->entries = eager_buffer;

  // we bzero this out so that valgrind doesn't believe these are
  // "uninitialized". They get filled in via RDMA so valgrind doesn't
  // know that the values have been filled in
  memset(new->entries, 0, sizeof(struct photon_rdma_eager_buf_entry_t) * size);

  new->curr = 0;
  new->num_entries = size;

  return new;

error_exit:
  return NULL;
}

void photon_rdma_eager_buf_free(photonEagerBuf buf) {
  free(buf);
}
