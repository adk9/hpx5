#include <stdlib.h>
#include <string.h>

#define USING_DAPL1

#ifdef USING_DAPL1
  #include <dat/udat.h>
#else
  #include <dat2/udat.h>
#endif

#include "dapl_buffer.h"
#include "logging.h"

void log_dat_err(const DAT_RETURN status, const char *fmt, ...);
void log_dto_err(const DAT_DTO_COMPLETION_STATUS status, const char *fmt, ...);

dapl_buffer_t *dapl_buffer_create(char *buf, int size) {
	dapl_buffer_t *new_buf;

	dbg_info("dapl_buffer_create()");

	new_buf = malloc(sizeof(dapl_buffer_t));
	if (!new_buf) {
	        log_err("dapl_buffer_create(): malloc failed");
		return NULL;
	}

	bzero(new_buf, sizeof(*new_buf));

	dbg_info("dapl_buffer_create(): alloc'd buffer: %p", new_buf);

	new_buf->buffer = buf;
	new_buf->size = size;
	new_buf->ref_count = 1;

	return new_buf;
}

void dapl_buffer_free(dapl_buffer_t *buffer) {
	if (buffer->is_registered)
		dapl_buffer_unregister(buffer);

	free(buffer);
}

int dapl_buffer_register(dapl_buffer_t *dbuffer, DAT_IA_HANDLE ia, DAT_PZ_HANDLE pz) {
	DAT_LMR_HANDLE handle;
	DAT_LMR_CONTEXT lmr_context;
	DAT_RMR_CONTEXT rmr_context;
	DAT_REGION_DESCRIPTION rd;
	DAT_MEM_PRIV_FLAGS flags;
	DAT_RETURN retval;

	dbg_info("dapl_buffer_register(): %p", dbuffer);

	if (dbuffer->is_registered)
		return 0;

	dbg_info("dapl_buffer_register(): buffer needs to be registered");

	if (dbuffer->is_registered)
		dapl_buffer_unregister(dbuffer);

	rd.for_va = dbuffer->buffer;
	flags = DAT_MEM_PRIV_LOCAL_READ_FLAG | DAT_MEM_PRIV_LOCAL_WRITE_FLAG;

	flags |= DAT_MEM_PRIV_REMOTE_READ_FLAG | DAT_MEM_PRIV_REMOTE_WRITE_FLAG;

	dbg_info("dapl_buffer_register(): creating local memory region");

	retval = dat_lmr_create(ia, DAT_MEM_TYPE_VIRTUAL, rd, dbuffer->size, pz, flags, 
#ifdef USING_DAPL1
#else
                            DAT_VA_TYPE_VA,
#endif
                            &handle, &lmr_context, &rmr_context, NULL, NULL);
	if (retval != DAT_SUCCESS) {
		log_dat_err(retval, "Couldn't allocate local memory region");
		goto error_exit;
	}

	dbg_info("dapl_buffer_register(): created local memory region");

	dbuffer->is_registered = 1;
	dbuffer->lmr_handle = handle;
	dbuffer->lmr_context = lmr_context;
	dbuffer->rmr_context = rmr_context;
	dbuffer->ia = ia;
	dbuffer->pz = pz;

	return 0;

error_exit:
	dbuffer->is_registered = 0;
	dbuffer->lmr_handle = DAT_HANDLE_NULL;
	dbuffer->lmr_context = 0;
	dbuffer->rmr_context = 0;
	return -1;
}

int dapl_buffer_unregister(dapl_buffer_t *dbuffer) {
	DAT_RETURN retval;

	dbg_info("dapl_buffer_unregister()");

	retval = dat_lmr_free(dbuffer->lmr_handle);
	if (retval == DAT_INVALID_STATE) {
		log_err("buffer is currently pinned");
		return -1;
	} else if (retval != DAT_SUCCESS) {
		log_err("Couldn't unregister buffer!");
	}
	else if (retval != DAT_SUCCESS) {
		log_dat_err(retval,"dat_lmr_free() failed");
		return -1;
	}


	dbuffer->is_registered = 0;
	dbuffer->lmr_handle = DAT_HANDLE_NULL;
	dbuffer->lmr_context = 0;
	dbuffer->rmr_context = 0;

	return 0;
}
