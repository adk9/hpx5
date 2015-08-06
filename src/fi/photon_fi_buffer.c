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

  dbuffer->buf.priv.key0 = 0;
  dbuffer->buf.priv.key1 = 0;
  dbuffer->priv_ptr = (void*)NULL;
  dbuffer->priv_size = sizeof(void*);
  dbuffer->is_registered = 1;

  return PHOTON_OK;

error_exit:
  dbuffer->is_registered = 0;
  return PHOTON_ERROR;
}

static int __fi_buffer_unregister(photonBI dbuffer, void *ctx) {
  int retval;

  dbg_trace("buffer address: %p", dbuffer);


  dbuffer->is_registered = 0;

  return PHOTON_OK;
}
