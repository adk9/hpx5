#include <stdlib.h>
#include <string.h>

#include "photon_ugni_buffer.h"
#include "photon_ugni_connect.h"
#include "logging.h"

static int __ugni_buffer_register(photonBI dbuffer, void *ctx);
static int __ugni_buffer_unregister(photonBI dbuffer, void *ctx);

struct photon_buffer_interface_t ugni_buffer_interface = {
  .buffer_create = _photon_buffer_create,
  .buffer_free = _photon_buffer_free,
  .buffer_register = __ugni_buffer_register,
  .buffer_unregister = __ugni_buffer_unregister,
};

static int __ugni_buffer_register(photonBI dbuffer, void *ctx) {
  int status;
  gni_mem_handle_t mdh, *smdh;

  if (dbuffer->is_registered)
    return PHOTON_OK;

  status = GNI_MemRegister(((ugni_cnct_ctx*)ctx)->nic_handle, (uint64_t)dbuffer->buf.addr,
			   dbuffer->buf.size, NULL, GNI_MEM_READWRITE, -1, &mdh);
  if (status != GNI_RC_SUCCESS) {
    dbg_err("GNI_MemRegister ERROR status: %s (%d)", gni_err_str[status], status);
    goto error_exit;
  }

  dbg_trace("GNI_MemRegister size: %lu address: %p", dbuffer->buf.size, dbuffer->buf.addr);

  smdh = malloc(sizeof(mdh));
  *smdh = mdh;

  dbuffer->buf.priv.key0 = mdh.qword1;
  dbuffer->buf.priv.key1 = mdh.qword2;
  dbuffer->priv_ptr = smdh;
  dbuffer->priv_size = sizeof(*smdh);
  dbuffer->is_registered = 1;

  return PHOTON_OK;

error_exit:
  dbuffer->is_registered = 0;
  return PHOTON_ERROR;
}

static int __ugni_buffer_unregister(photonBI dbuffer, void *ctx) {
  int status;

  status = GNI_MemDeregister(((ugni_cnct_ctx*)ctx)->nic_handle, (gni_mem_handle_t*)dbuffer->priv_ptr);
  if (status != GNI_RC_SUCCESS) {
    dbg_err("GNI_MemDeregister ERROR status: %s (%d)", gni_err_str[status], status);
    goto error_exit;
  }
  dbg_trace("GNI_MemDeregister (%p)", (void *)dbuffer->buf.addr);

  dbuffer->is_registered = 0;

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}
