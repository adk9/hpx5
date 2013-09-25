#ifndef PHOTON_BUFFERTABLE_H
#define PHOTON_BUFFERTABLE_H

#include <stdlib.h>

#include "photon_buffer.h"
#include "logging.h"

/* return nonzero means error */
int buffertable_init(int max_buffers);
void buffertable_finalize();

int buffertable_find_containing(void* start, int size, photonBuffer* result);
int buffertable_find_exact(void* start, int size, photonBuffer* result);

int buffertable_insert(photonBuffer buffer);
int buffertable_remove(photonBuffer buffer);

#endif

