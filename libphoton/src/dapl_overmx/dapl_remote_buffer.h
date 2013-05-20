#ifndef DAPL_REMOTE_BUFFER_H
#define DAPL_REMOTE_BUFFER_H

#include <stdint.h>

#include <dat/udat.h>

typedef struct dapl_remote_buffer {
	uint32_t request;
	DAT_RMR_CONTEXT context;
	DAT_VADDR address;
	uint32_t size;
	int tag;
} dapl_remote_buffer_t;

dapl_remote_buffer_t *dapl_remote_buffer_create();
void dapl_remote_buffer_free(dapl_remote_buffer_t *drb);

#endif
