#include <stdlib.h>
#include <string.h>

#include "verbs_buffer.h"
#include "logging.h"

//void log_dat_err(const DAT_RETURN status, const char *fmt, ...);
//void log_dto_err(const DAT_DTO_COMPLETION_STATUS status, const char *fmt, ...);

verbs_buffer_t *__verbs_buffer_create(char *buf, int size) {
	verbs_buffer_t *new_buf;

	dbg_info("verbs_buffer_create()");

	new_buf = malloc(sizeof(verbs_buffer_t));
	if (!new_buf) {
		log_err("verbs_buffer_create(): malloc failed");
		return NULL;
	}

	bzero(new_buf, sizeof(*new_buf));

	dbg_info("verbs_buffer_create(): alloc'd buffer: %p", new_buf);

	new_buf->buffer = buf;
	new_buf->size = size;
	new_buf->ref_count = 1;

	return new_buf;
}

void __verbs_buffer_free(verbs_buffer_t *buffer) {
	if (buffer->is_registered)
		__verbs_buffer_unregister(buffer);

	free(buffer);
}

int __verbs_buffer_register(verbs_buffer_t *dbuffer, struct ibv_pd *pd) {
	enum ibv_access_flags flags;

	dbg_info("verbs_buffer_register(): %p", dbuffer);

	if (dbuffer->is_registered)
		return 0;

	flags = IBV_ACCESS_LOCAL_WRITE|IBV_ACCESS_REMOTE_READ|IBV_ACCESS_REMOTE_WRITE;
	dbuffer->mr = ibv_reg_mr(pd, dbuffer->buffer, dbuffer->size, flags);
	if (!dbuffer->mr) {
		fprintf(stderr, "Couldn't allocate MR\n");
		goto error_exit;
	}

	dbuffer->is_registered = 1;
	return 0;

error_exit:
	dbuffer->is_registered = 0;
	dbuffer->mr = NULL;
	return -1;
}

int __verbs_buffer_unregister(verbs_buffer_t *dbuffer) {
	int retval;

	dbg_info("verbs_buffer_unregister()");

	retval = ibv_dereg_mr(dbuffer->mr);
	if(retval) {
		return retval;
	}

	// GFR: This segfaults and doesn't quite make sense?
	//if(dbuffer->mr){
	//    free(dbuffer->mr);
	//}

	dbuffer->is_registered = 0;
	dbuffer->mr = NULL;

	return 0;
}
