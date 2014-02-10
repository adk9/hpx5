#include "photon_buffertable.h"

static photonBuffer* registered_buffers=NULL;
static int buffertable_size;
static int num_registered_buffers;

static int buffertable_resize(int new_buffertable_size) {
  photonBuffer* new_registered_buffer = realloc(registered_buffers,sizeof(struct photon_buffer_t*)*new_buffertable_size);
  if (!new_registered_buffer)
    return 1;
  registered_buffers = new_registered_buffer;
  buffertable_size = new_buffertable_size;
  return 0;
}
int buffertable_init(int num_buffers) {
  if (!registered_buffers || buffertable_size < num_buffers)
    if (buffertable_resize(num_buffers))
      return 1;
  return 0;
}
void buffertable_finalize() {
  free(registered_buffers);
  registered_buffers = NULL;
  num_registered_buffers = 0;
  buffertable_size = 0;
}

int buffertable_find_containing(void* start, int size, photonBuffer* result) {
  int i, cond;

  for(i=0; i<num_registered_buffers; i++) {
    photonBuffer tmpbuf = registered_buffers[i];
    cond =  ((void *)(tmpbuf->buffer) <= start);
    cond &= ((void *)(tmpbuf->buffer+tmpbuf->size) >= start+size);
    if ( cond ) {
      if( result )
        *result = tmpbuf;
      return 0;
    }
  }
  return 1;
}

int buffertable_find_exact(void* start, int size, photonBuffer* result) {
  int i, cond;

  for(i=0; i<num_registered_buffers; i++) {
    photonBuffer tmpbuf = registered_buffers[i];
    cond =  ((void *)(tmpbuf->buffer) == start);
    cond &= (tmpbuf->size == size);
    if ( cond ) {
      if( result )
        *result = tmpbuf;
      return 0;
    }
  }
  return 1;
}

int buffertable_insert(photonBuffer buffer) {
  if (!registered_buffers) {
    log_err("buffertable_insert(): Buffertable not initialized. Call buffertable_init() first.");
    return 1;
  }
  if (num_registered_buffers >= buffertable_size) {
    if(buffertable_resize(buffertable_size*2))
      return 2;
  }
  registered_buffers[num_registered_buffers++]=buffer;
  return 0;
}


int buffertable_remove(photonBuffer buffer) {
  int i;

  if (!registered_buffers) {
    log_err("buffertable_insert(): Buffertable not initialized. Call buffertable_init() first.");
    return 1;
  }

  for(i=0; i<num_registered_buffers; i++) {
    photonBuffer tmpbuf = registered_buffers[i];
    if ( tmpbuf == buffer ) {
      registered_buffers[i] = registered_buffers[--num_registered_buffers];
      return 0;
    }
  }

  return 1;
}

