#ifndef PHOTON_BUFFER_H
#define PHOTON_BUFFER_H

#include <stdint.h>
#include "config.h"
#include "photon.h"

typedef struct photon_buffer_internal_t {
  struct photon_buffer_t buf;
  uint32_t request;
  int tag;
  int is_registered;
  int ref_count;
  /* keep a reference to the registered mem handle */
  void *priv_ptr;
  int priv_size;
} photon_buffer_internal;

typedef struct photon_buffer_internal_t * photonBI;

struct photon_buffer_interface_t {
  photonBI (*buffer_create)(void *buf, uint64_t size);
  void (*buffer_free)(photonBI buf);
  int (*buffer_register)(photonBI buf, void *ctx);
  int (*buffer_unregister)(photonBI buf, void *ctx);
  int (*buffer_get_private)(photonBI buf, photonBufferPriv ret_priv);
};

typedef struct photon_buffer_interface_t * photonBufferInterface;

/* internal buffer API */
void photon_buffer_init(photonBufferInterface buf_interface);
photonBI photon_buffer_create(void *buf, uint64_t size);
void photon_buffer_free(photonBI buf);
int photon_buffer_register(photonBI buf, void *ctx);
int photon_buffer_unregister(photonBI buf, void *ctx);
int photon_buffer_get_private(photonBI buf, photonBufferPriv ret_priv);

/* default buffer interface methods */
photonBI _photon_buffer_create(void *buf, uint64_t size);
void _photon_buffer_free(photonBI buf);
int _photon_buffer_register(photonBI buf, void *ctx);
int _photon_buffer_unregister(photonBI buf, void *ctx);

#endif
