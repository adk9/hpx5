#ifndef VERBS_REMOTE_BUFFER_H
#define VERBS_REMOTE_BUFFER_H

#include <stdint.h>

typedef struct verbs_remote_buffer {
	uint32_t request;
	uintptr_t addr;
	uint32_t  lkey;
	uint32_t  rkey;
	uint32_t size;
	int tag;
} verbs_remote_buffer_t;

verbs_remote_buffer_t *__verbs_remote_buffer_create();
void __verbs_remote_buffer_free(verbs_remote_buffer_t *drb);

#endif
