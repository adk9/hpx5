#include <stdlib.h>
#include <string.h>

#include "photon_ugni_buffer.h"
#include "logging.h"

struct photon_buffer_interface_t ugni_buffer_interface = {
	.buffer_create = _photon_buffer_create,
	.buffer_free = _photon_buffer_free,
	.buffer_register = __ugni_buffer_register,
	.buffer_unregister = __ugni_buffer_unregister
};

int __ugni_buffer_register(photonBuffer dbuffer) {
	
	return PHOTON_OK;
}

int __ugni_buffer_unregister(photonBuffer dbuffer) {

	return PHOTON_OK;
}
