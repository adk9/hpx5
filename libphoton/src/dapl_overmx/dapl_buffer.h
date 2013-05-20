#ifndef DAPL_BUFFER_H
#define DAPL_BUFFER_H

#define USING_DAPL1

#ifdef USING_DAPL1
  #include <dat/udat.h>
#else
  #include <dat2/udat.h>
#endif

typedef struct dapl_buffer {
	char *buffer;
	int size;
	int is_registered;
//	int is_remote;
	int ref_count;
	DAT_LMR_HANDLE lmr_handle;
	DAT_LMR_CONTEXT lmr_context;
	DAT_RMR_CONTEXT rmr_context;

	DAT_IA_HANDLE ia;
	DAT_PZ_HANDLE pz;
} dapl_buffer_t;

dapl_buffer_t *dapl_buffer_create(char *buf, int size);
void dapl_buffer_free(dapl_buffer_t *buffer);
int dapl_buffer_register(dapl_buffer_t *dbuffer, DAT_IA_HANDLE ia, DAT_PZ_HANDLE pz);
int dapl_buffer_unregister(dapl_buffer_t *dbuffer);

#endif
