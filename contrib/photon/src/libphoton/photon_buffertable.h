#ifndef PHOTON_BUFFERTABLE_H
#define PHOTON_BUFFERTABLE_H

#include <stdlib.h>

#include "photon_buffer.h"
#include "logging.h"

/* return nonzero means error */
PHOTON_INTERNAL int buffertable_init(int max_buffers);
PHOTON_INTERNAL void buffertable_finalize();

PHOTON_INTERNAL int buffertable_find_containing(void* start, uint64_t size, photonBI* result);
PHOTON_INTERNAL int buffertable_find_exact(void* start, uint64_t size, photonBI* result);

PHOTON_INTERNAL int buffertable_insert(photonBI buffer);
PHOTON_INTERNAL int buffertable_remove(photonBI buffer);

#endif

