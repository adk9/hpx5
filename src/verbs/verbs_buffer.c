#include <stdlib.h>
#include <string.h>

#include <infiniband/verbs.h>

#include "verbs_buffer.h"
#include "verbs_connect.h"
#include "logging.h"

extern verbs_cnct_ctx verbs_ctx;

struct photon_buffer_interface_t verbs_buffer_interface = {
	.buffer_create = _photon_buffer_create,
	.buffer_free = _photon_buffer_free,
	.buffer_register = __verbs_buffer_register,
	.buffer_unregister = __verbs_buffer_unregister
};

int __verbs_buffer_register(photonBuffer dbuffer) {
	enum ibv_access_flags flags;

	dbg_info("buffer address: %p", dbuffer);

	if (dbuffer->is_registered)
		return 0;

	flags = IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_READ|IBV_ACCESS_REMOTE_WRITE;
	dbuffer->mr = ibv_reg_mr(verbs_ctx.ib_pd, dbuffer->buffer, dbuffer->size, flags);
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

int __verbs_buffer_unregister(photonBuffer dbuffer) {
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
