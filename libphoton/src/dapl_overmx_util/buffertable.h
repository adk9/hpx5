#ifndef BUFFERTABLE_H
#define BUFFERTABLE_H

#include <stdlib.h>
#include <dapl_buffer.h>
#include <logging.h>

// return nonzero means error

int buffertable_init(int max_buffers);
void buffertable_finalize();

int buffertable_find_containing(void* start, int size, dapl_buffer_t** result);
int buffertable_find_exact(void* start, int size, dapl_buffer_t** result);

int buffertable_insert(dapl_buffer_t* buffer);
int buffertable_remove(dapl_buffer_t *buffer);

#endif

