#ifndef PHOTON_BUFFER_H
#define PHOTON_BUFFER_H

#include <stdint.h>
#include "config.h"
#include "photon.h"

#define BUFFER_FLAG_NIL     0x0000
#define BUFFER_FLAG_NOTIFY  (1<<1)

struct photon_acct_t {
  volatile uint64_t rcur;
  uint32_t rloc;
  uint32_t event_prefix;
};

typedef struct photon_buffer_internal_t {
  struct photon_buffer_t buf;
  uint16_t is_registered;
  uint16_t ref_count;
  /* keep a reference to the registered mem handle */
  void *priv_ptr;
  int priv_size;
} photon_buffer_internal;

typedef struct photon_buffer_internal_t * photonBI;

struct photon_buffer_interface_t {
  photonBI (*buffer_create)(void *buf, uint64_t size);
  void (*buffer_free)(photonBI buf);
  int (*buffer_register)(photonBI buf, void *ctx, int flags);
  int (*buffer_unregister)(photonBI buf, void *ctx);
};

typedef struct photon_buffer_interface_t * photonBufferInterface;

/* internal buffer API */
PHOTON_INTERNAL void photon_buffer_init(photonBufferInterface buf_interface);
PHOTON_INTERNAL photonBI photon_buffer_create(void *buf, uint64_t size);
PHOTON_INTERNAL void photon_buffer_free(photonBI buf);
PHOTON_INTERNAL int photon_buffer_register(photonBI buf, void *ctx, int flags);
PHOTON_INTERNAL int photon_buffer_unregister(photonBI buf, void *ctx);

/* default buffer interface methods */
PHOTON_INTERNAL photonBI _photon_buffer_create(void *buf, uint64_t size);
PHOTON_INTERNAL void _photon_buffer_free(photonBI buf);
PHOTON_INTERNAL int _photon_buffer_register(photonBI buf, void *ctx, int flags);
PHOTON_INTERNAL int _photon_buffer_unregister(photonBI buf, void *ctx);

#endif
