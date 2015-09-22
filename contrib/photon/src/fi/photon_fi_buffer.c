#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "photon_fi_buffer.h"
#include "photon_fi_connect.h"
#include "logging.h"

static int __fi_buffer_register(photonBI dbuffer, void *ctx, int flags);
static int __fi_buffer_unregister(photonBI dbuffer, void *ctx);

struct photon_buffer_interface_t fi_buffer_interface = {
  .buffer_create = _photon_buffer_create,
  .buffer_free = _photon_buffer_free,
  .buffer_register = __fi_buffer_register,
  .buffer_unregister = __fi_buffer_unregister,
};

static int __fi_buffer_register(photonBI dbuffer, void *ctx, int flags) {
  struct fid_mr *mr;
  fi_cnct_ctx *c = (fi_cnct_ctx*)ctx;
  int rc;

  dbg_trace("Registering buffer at addr %p of size %lu",
	    (void*)dbuffer->buf.addr, dbuffer->buf.size);
  
  rc = fi_mr_reg(c->dom, (void*)dbuffer->buf.addr, dbuffer->buf.size,
		 FI_WRITE|FI_READ|FI_REMOTE_WRITE|FI_REMOTE_READ,
		 0, 0, 0, &mr, NULL);
  if (rc) {
    dbg_err("Could not register memory at %p, size %lu: %s",
	    (void*)dbuffer->buf.addr, dbuffer->buf.size, fi_strerror(-rc));
    goto error_exit;
  }

  dbuffer->buf.priv.key0 = fi_mr_key(mr);
  dbuffer->buf.priv.key1 = fi_mr_key(mr);
  dbuffer->priv_ptr = (void*)mr;
  dbuffer->priv_size = sizeof(*mr);
  dbuffer->is_registered = 1;

  return PHOTON_OK;

error_exit:
  dbuffer->is_registered = 0;
  return PHOTON_ERROR;
}

static int __fi_buffer_unregister(photonBI dbuffer, void *ctx) {
  int rc;
  
  rc = fi_close(&((struct fid_mr*)dbuffer->priv_ptr)->fid);
  if (rc) {
    dbg_err("Could not deregister memory region: %s", fi_strerror(-rc));
    goto error_exit;
  }

  dbuffer->is_registered = 0;

  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}
