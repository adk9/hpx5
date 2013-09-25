#ifndef PHOTON_BUFFER_H
#define PHOTON_BUFFER_H

#include <stdint.h>

#ifdef HAVE_VERBS
#include <infiniband/verbs.h>
#endif

#include "libphoton.h"

typedef struct photon_buffer_t {
	void *buffer;
	uint64_t size;
	int is_registered;
	int ref_count;

#ifdef HAVE_VERBS
	struct ibv_mr *mr;
#endif
} photon_buffer;

typedef struct photon_remote_buffer_t {
	uint32_t  request;
	uintptr_t addr;
	uint32_t  lkey;
	uint32_t  rkey;
	uint32_t  size;
	int       tag;
} photon_remote_buffer;

typedef struct photon_buffer_t * photonBuffer;
typedef struct photon_remote_buffer_t * photonRemoteBuffer;

struct photon_buffer_interface_t {
	photonBuffer (*buffer_create)(void *buf, uint64_t size);
	void (*buffer_free)(photonBuffer buf);
	int (*buffer_register)(photonBuffer buf);
	int (*buffer_unregister)(photonBuffer buf);
};

typedef struct photon_buffer_interface_t * photonBufferInterface;

/* internal buffer API */
void photon_buffer_init(photonBufferInterface buf_interface);
photonBuffer photon_buffer_create(void *buf, uint64_t size);
void photon_buffer_free(photonBuffer buf);
int photon_buffer_register(photonBuffer buf);
int photon_buffer_unregister(photonBuffer buf);

/* including the remote buffers */
photonRemoteBuffer photon_remote_buffer_create();
void photon_remote_buffer_free(photonRemoteBuffer drb);

/* default buffer interface methods */
photonBuffer _photon_buffer_create(void *buf, uint64_t size);
void _photon_buffer_free(photonBuffer buf);
int _photon_buffer_register(photonBuffer buf);
int _photon_buffer_unregister(photonBuffer buf);

#endif
