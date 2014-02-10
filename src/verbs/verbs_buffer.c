#include <stdlib.h>
#include <string.h>

#include <infiniband/verbs.h>

#include "verbs_buffer.h"
#include "verbs_connect.h"
#include "logging.h"

static int __verbs_buffer_register(photonBuffer dbuffer, void *ctx);
static int __verbs_buffer_unregister(photonBuffer dbuffer, void *ctx);
static int __verbs_buffer_get_private(photonBuffer buf, photonBufferPriv ret_priv);

struct photon_buffer_interface_t verbs_buffer_interface = {
  .buffer_create = _photon_buffer_create,
  .buffer_free = _photon_buffer_free,
  .buffer_register = __verbs_buffer_register,
  .buffer_unregister = __verbs_buffer_unregister,
  .buffer_get_private = __verbs_buffer_get_private
};

static int __verbs_buffer_register(photonBuffer dbuffer, void *ctx) {
  enum ibv_access_flags flags;

  dbg_info("buffer address: %p", dbuffer);

  if (dbuffer->is_registered)
    return 0;

  flags = IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_READ|IBV_ACCESS_REMOTE_WRITE;
  dbuffer->mr = ibv_reg_mr(((verbs_cnct_ctx*)ctx)->ib_pd, dbuffer->buffer, dbuffer->size, flags);
  if (!dbuffer->mr) {
    log_err("Could not allocate MR\n");
    goto error_exit;
  }

  dbuffer->is_registered = 1;
  return 0;

error_exit:
  dbuffer->is_registered = 0;
  dbuffer->mr = NULL;
  return -1;
}

static int __verbs_buffer_unregister(photonBuffer dbuffer, void *ctx) {
  int retval;

  dbg_info();

  retval = ibv_dereg_mr(dbuffer->mr);
  if(retval) {
    return retval;
  }

  dbuffer->is_registered = 0;
  dbuffer->mr = NULL;

  return 0;
}

static int __verbs_buffer_get_private(photonBuffer buf, photonBufferPriv ret_priv) {
  if (buf->is_registered) {
    (*ret_priv).key0 = buf->mr->lkey;
    (*ret_priv).key1 = buf->mr->rkey;
    return PHOTON_OK;
  }

  return PHOTON_ERROR;
}
