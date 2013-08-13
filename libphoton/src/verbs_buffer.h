#ifndef VERBS_BUFFER_H
#define VERBS_BUFFER_H

#include <infiniband/verbs.h>

typedef struct verbs_buffer {
	char *buffer;
	int size;
	int is_registered;
//    int is_remote;
	int ref_count;
//    DAT_LMR_HANDLE lmr_handle;
//    DAT_LMR_CONTEXT lmr_context;
//    DAT_RMR_CONTEXT rmr_context;
	struct ibv_mr *mr;

////*    DAT_IA_HANDLE ia;
////*    DAT_PZ_HANDLE pz;
} verbs_buffer_t;

verbs_buffer_t *verbs_buffer_create(char *buf, int size);
void verbs_buffer_free(verbs_buffer_t *buffer);
int verbs_buffer_register(verbs_buffer_t *dbuffer, struct ibv_pd *pd);
int verbs_buffer_unregister(verbs_buffer_t *dbuffer);

#endif
