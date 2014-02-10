#include <stdlib.h>
#include <string.h>

#include "photon_ugni_buffer.h"
#include "photon_ugni_connect.h"
#include "logging.h"

static int __ugni_buffer_register(photonBuffer dbuffer, void *ctx);
static int __ugni_buffer_unregister(photonBuffer dbuffer, void *ctx);
static int __ugni_buffer_get_private(photonBuffer buf, photonBufferPriv ret_priv);

struct photon_buffer_interface_t ugni_buffer_interface = {
  .buffer_create = _photon_buffer_create,
  .buffer_free = _photon_buffer_free,
  .buffer_register = __ugni_buffer_register,
  .buffer_unregister = __ugni_buffer_unregister,
  .buffer_get_private = __ugni_buffer_get_private
};

static int __ugni_buffer_register(photonBuffer dbuffer, void *ctx) {
  int status;

  if (dbuffer->is_registered)
    return PHOTON_OK;

  status = GNI_MemRegister(((ugni_cnct_ctx*)ctx)->nic_handle, (uint64_t)dbuffer->buffer, dbuffer->size,
                           NULL, GNI_MEM_READWRITE, -1, &dbuffer->mdh);
  if (status != GNI_RC_SUCCESS) {
    dbg_err("GNI_MemRegister ERROR status: %s (%d)", gni_err_str[status], status);
    goto error_exit;
  }

  dbg_info("GNI_MemRegister size: %lu address: %p", dbuffer->size, dbuffer->buffer);

  dbuffer->is_registered = 1;

  return PHOTON_OK;

error_exit:
  dbuffer->is_registered = 0;
  return PHOTON_ERROR;
}

static int __ugni_buffer_unregister(photonBuffer dbuffer, void *ctx) {
  int status;

  status = GNI_MemDeregister(((ugni_cnct_ctx*)ctx)->nic_handle, &dbuffer->mdh);
  if (status != GNI_RC_SUCCESS) {
    dbg_err("GNI_MemDeregister ERROR status: %s (%d)", gni_err_str[status], status);
    goto error_exit;
  }
  dbg_info("GNI_MemDeregister (%p)", (void *)dbuffer->buffer);

  dbuffer->is_registered = 0;

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int __ugni_buffer_get_private(photonBuffer buf, photonBufferPriv ret_priv) {
  if (buf->is_registered) {
    (*ret_priv).key0 = buf->mdh.qword1;
    (*ret_priv).key1 = buf->mdh.qword2;
    return PHOTON_OK;
  }

  return PHOTON_ERROR;
}
