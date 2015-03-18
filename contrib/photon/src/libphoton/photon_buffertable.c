#include "photon_buffertable.h"
#include "libsync/locks.h"

static photonBI* registered_buffers=NULL;
static int buffertable_size;
static int num_registered_buffers;
static tatas_lock_t bt_lock;

static int buffertable_resize(int new_buffertable_size) {
  int newsize = sizeof(struct photon_buffer_internal_t *) * new_buffertable_size;
  photonBI* new_registered_buffer = realloc(registered_buffers, newsize);
  if (!new_registered_buffer)
    return PHOTON_ERROR;
  registered_buffers = new_registered_buffer;
  buffertable_size = new_buffertable_size;
  return PHOTON_OK;
}

int buffertable_init(int num_buffers) {
  if (!registered_buffers || buffertable_size < num_buffers)
    if (buffertable_resize(num_buffers))
      return PHOTON_ERROR;
  sync_tatas_init(&bt_lock);
  return PHOTON_OK;
}

void buffertable_finalize() {
  free(registered_buffers);
  registered_buffers = NULL;
  num_registered_buffers = 0;
  buffertable_size = 0;
}

int buffertable_find_containing(void* start, uint64_t size, photonBI* result) {
  int i, cond;

  sync_tatas_acquire(&bt_lock);
  {
    for(i=0; i<num_registered_buffers; i++) {
      photonBI tmpbuf = registered_buffers[i];
      cond =  ((void *)(tmpbuf->buf.addr) <= start);
      cond &= ((char *)(tmpbuf->buf.addr+tmpbuf->buf.size) >= (char *)start+size);
      if ( cond ) {
	if( result )
	  *result = tmpbuf;
	sync_tatas_release(&bt_lock);
	return PHOTON_OK;
      }
    }
  }
  sync_tatas_release(&bt_lock);

  return PHOTON_ERROR;
}

int buffertable_find_exact(void* start, uint64_t size, photonBI* result) {
  int i, cond;

  sync_tatas_acquire(&bt_lock);
  {
    for(i=0; i<num_registered_buffers; i++) {
      photonBI tmpbuf = registered_buffers[i];
      cond =  ((void *)(tmpbuf->buf.addr) == start);
      cond &= (tmpbuf->buf.size == size);
      if ( cond ) {
	if( result )
	  *result = tmpbuf;
	sync_tatas_release(&bt_lock);
	return PHOTON_OK;
      }
    }
  }
  sync_tatas_release(&bt_lock);
  return PHOTON_ERROR;
}

int buffertable_insert(photonBI buffer) {
  if (!registered_buffers) {
    log_err("buffertable_insert(): Buffertable not initialized. Call buffertable_init() first.");
    return PHOTON_ERROR;
  }

  sync_tatas_acquire(&bt_lock);
  {
    if (num_registered_buffers >= buffertable_size) {
      if (buffertable_resize(buffertable_size*2) != PHOTON_OK)
	return PHOTON_ERROR_RESOURCE;
    }
    registered_buffers[num_registered_buffers++]=buffer;
  }
  sync_tatas_release(&bt_lock);

  return PHOTON_OK;
}


int buffertable_remove(photonBI buffer) {
  int i;

  if (!registered_buffers) {
    log_err("buffertable_insert(): Buffertable not initialized. Call buffertable_init() first.");
    return PHOTON_ERROR;
  }

  sync_tatas_acquire(&bt_lock);
  {
    for(i=0; i<num_registered_buffers; i++) {
      photonBI tmpbuf = registered_buffers[i];
      if ( tmpbuf == buffer ) {
	registered_buffers[i] = registered_buffers[--num_registered_buffers];
	sync_tatas_release(&bt_lock);
	return PHOTON_OK;
      }
    }
  }
  sync_tatas_release(&bt_lock);

  return PHOTON_ERROR;
}

