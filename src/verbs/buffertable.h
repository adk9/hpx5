#ifndef BUFFERTABLE_H
#define BUFFERTABLE_H

#include <stdlib.h>
#include <verbs_buffer.h>
#include <logging.h>

// return nonzero means error

int buffertable_init(int max_buffers);
void buffertable_finalize();

int buffertable_find_containing(void* start, int size, verbs_buffer_t** result);
int buffertable_find_exact(void* start, int size, verbs_buffer_t** result);

int buffertable_insert(verbs_buffer_t* buffer);
int buffertable_remove(verbs_buffer_t *buffer);

#endif

