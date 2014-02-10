#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "photon.h"
#include "libphoton.h"
#include "logging.h"
#include "photon_buffer.h"

static photonBufferInterface bi = NULL;

void photon_buffer_init(photonBufferInterface buf_interface) {
	if (buf_interface) {
		bi = buf_interface;
	}
	else {
		log_warn("Did not set buffer interface!");
	}
}

photonBuffer photon_buffer_create(void *buf, uint64_t size) {
	if (!bi) {
		log_err("Buffer interface not set!");
		return NULL;
	}

	return bi->buffer_create(buf, size);
}

void photon_buffer_free(photonBuffer buf) {
	if (!bi) {
		log_err("Biffer interface not set!");
		return;
	}

	return bi->buffer_free(buf);
}

int photon_buffer_register(photonBuffer buf, void *ctx) {
	if (!bi) {
		log_err("Buffer interface not set!");
		return PHOTON_ERROR;
	}

	return bi->buffer_register(buf, ctx);
}

int photon_buffer_unregister(photonBuffer buf, void *ctx) {
	if (!bi) {
		log_err("Buffer interface not set!");
		return PHOTON_ERROR;
	}
	
	return bi->buffer_unregister(buf, ctx);
}

int photon_buffer_get_private(photonBuffer buf, photonBufferPriv ret_priv) {
	if (!bi) {
		log_err("Buffer interface not set!");
		return PHOTON_ERROR;
	}
	
	return bi->buffer_get_private(buf, ret_priv);
}

/* remote buffers */
photonRemoteBuffer photon_remote_buffer_create() {
	photonRemoteBuffer drb;

	drb = malloc(sizeof(struct photon_remote_buffer_t));
	if (!drb)
		return NULL;

	memset(drb, 0, sizeof(struct photon_remote_buffer_t));

	drb->request = NULL_COOKIE;

	return drb;
}

void photon_remote_buffer_free(photonRemoteBuffer drb) {
	if (drb) {
		free(drb);
	}
}

/* default buffer interface methods */
photonBuffer _photon_buffer_create(void *buf, uint64_t size) {
	photonBuffer new_buf;
	
	dbg_info();
	
	new_buf = malloc(sizeof(struct photon_buffer_t));
	if (!new_buf) {
		log_err("malloc failed");
		return NULL;
	}

	memset(new_buf, 0, sizeof(struct photon_buffer_t));
	
	dbg_info("allocated buffer struct: %p", new_buf);
	dbg_info("contains buffer pointer: %p of size %" PRIu64, buf, size);

	new_buf->buffer = buf;
	new_buf->size = size;
	new_buf->ref_count = 1;

	return new_buf;
}

void _photon_buffer_free(photonBuffer buf) {	
	/*
	if (buf->is_registered) {
		if (!bi) {
			log_err("Buffer interface not set!");
			return;
		}
		bi->buffer_unregister(buf, ctx);
	}
	*/
	free(buf);
}

int _photon_buffer_register(photonBuffer buf, void *ctx) {
	if (!bi) {
		log_err("Buffer interface not set!");
		return PHOTON_ERROR;
	}

	return bi->buffer_register(buf, ctx);
}

int _photon_buffer_unregister(photonBuffer buf, void *ctx) {
	if (!bi) {
		log_err("Buffer interface not set!");
		return PHOTON_ERROR;
	}
	
	return bi->buffer_unregister(buf, ctx);
}
