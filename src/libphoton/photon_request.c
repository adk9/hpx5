#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "photon_backend.h"
#include "photon_request.h"

photonRequest photon_get_request(int proc) {
  photonRequestTable rt;
  photonRequest      req;
  uint32_t           req_curr;
  uint32_t           req_ind;
  uint32_t           tail;

  assert(proc >= 0 && proc < _photon_nproc);

  rt = photon_processes[proc].request_table;
  // skip index 0 since it is our NULL_COOKIE
  do {
    req_curr = sync_fadd(&rt->curr, 1, SYNC_RELAXED);
  } while (!(req_curr % rt->size));

  req_ind = req_curr % rt->size;
  tail = sync_load(&rt->tind, SYNC_RELAXED);

  if (req_ind == tail) {
    log_err("Request descriptors exhausted for proc %d, max=%u", proc, rt->size);
    return NULL;
  }

  req = &rt->reqs[req_ind];
  if (!req) {
    dbg_err("Uninitialized request pointer in request table");
    return NULL;
  }

  req->id = PROC_REQUEST_ID(proc, req_ind);
  req->index = req_ind;
  req->flags = REQUEST_FLAG_NIL;
  req->events = 0;
  //bit_array_clear_all(req->mmask);

  dbg_trace("Returning a new request (curr=%u) with id: 0x%016lx, tail=%u", req_curr, req->id, tail);

  return req;
}

photonRequest photon_lookup_request(photon_rid rid) {
  photonRequestTable rt;
  uint32_t proc, id;
  id = (uint32_t)(rid<<32>>32);
  proc = (uint32_t)(rid>>32);
  if (proc >= 0 && proc < _photon_nproc) {
    rt = photon_processes[proc].request_table;
  }
  else {
    log_err("Unknown proc (%u) obtained from rid: 0x%016lx", proc, rid);
    return NULL;
  }
  if (id >= 0 && id < rt->size) {
    return &rt->reqs[id];
  }
  else {
    log_err("Unknown request id (%u) obtained from rid: 0x%016lx", id, rid);
    return NULL;
  }
}

int photon_count_request(int proc) {
  photonRequestTable rt;
  uint32_t tind, curr;
  if (proc >= 0 && proc < _photon_nproc) {
    rt = photon_processes[proc].request_table;
    tind = sync_load(&rt->tind, SYNC_RELAXED);
    curr = sync_load(&rt->curr, SYNC_RELAXED);
    return (curr % rt->size) - (tind - rt->size);
  }
  else {
    return 0;
  }
}

int photon_free_request(photonRequest req) {
  photonRequestTable rt;
  uint32_t new_tind;
  rt = photon_processes[req->proc].request_table;
  new_tind = (req->index == (rt->size - 1))?0:req->index;
  sync_cas(&rt->tind, req->index-1, new_tind, SYNC_RELAXED, SYNC_RELAXED);
  return PHOTON_OK;
}

photonRequest photon_setup_request_direct(photonBuffer rbuf, int proc, int events) {
  photonRequest req;
  
  req = photon_get_request(proc);
  if (!req) {
    log_err("Couldn't allocate request");
    goto error_exit;
  }

  dbg_trace("Setting up a direct request: %d/0x%016lx/%p", proc, req->id, req);

  req->state = REQUEST_PENDING;
  req->proc = proc;
  req->type = EVQUEUE;
  req->tag = 0;
  req->length = rbuf->size;
  req->events = events;
  
  // fill in the internal buffer with the rbuf contents
  memcpy(&req->remote_buffer, rbuf, sizeof(*rbuf));
  // there is no matching request from the remote side, so fill in local values */
  req->remote_buffer.request = 0;
  req->remote_buffer.tag = 0;
  
  dbg_trace("Addr: %p", (void *)rbuf->addr);
  dbg_trace("Size: %lu", rbuf->size);
  dbg_trace("Keys: 0x%016lx / 0x%016lx", rbuf->priv.key0, rbuf->priv.key1);

  return req;
  
 error_exit:
  return NULL;
}

photonRequest photon_setup_request_ledger_info(photonRILedgerEntry ri_entry, int curr, int proc) {
  photonRequest req;

  req = photon_get_request(proc);
  if (!req) {
    log_err("Couldn't allocate request\n");
    goto error_exit;
  }

  dbg_trace("Setting up a new send buffer request: %d/0x%016lx/%p", proc, req->id, req);

  req->state = REQUEST_NEW;
  req->type = EVQUEUE;
  req->proc = proc;
  req->curr = curr;
  req->flags = ri_entry->flags;
  req->length = ri_entry->size;
  req->events = 1;

  /* save the remote buffer in the request */
  req->remote_buffer.request   = ri_entry->request;
  req->remote_buffer.tag       = ri_entry->tag;
  req->remote_buffer.buf.addr  = ri_entry->addr;
  req->remote_buffer.buf.size  = ri_entry->size;
  req->remote_buffer.buf.priv  = ri_entry->priv;
  
  dbg_trace("Remote request: 0x%016lx", ri_entry->request);
  dbg_trace("Addr: %p", (void *)ri_entry->addr);
  dbg_trace("Size: %lu", ri_entry->size);
  dbg_trace("Tag: %d",	ri_entry->tag);
  dbg_trace("Keys: 0x%016lx / 0x%016lx", ri_entry->priv.key0, ri_entry->priv.key1);

  // reset the info ledger entry
  ri_entry->header = 0;
  ri_entry->footer = 0;
  
  return req;
  
 error_exit:
  return NULL;
}

photonRequest photon_setup_request_ledger_eager(photonLedgerEntry entry, int curr, int proc) {
  photonRequest req;

  req = photon_get_request(proc);
  if (!req) {
    log_err("Couldn't allocate request\n");
    goto error_exit;
  }

  dbg_trace("Setting up a new eager buffer request: %d/0x%016lx/%p", proc, req->id, req);

  req->state = REQUEST_NEW;
  req->type = EVQUEUE;
  req->proc = proc;
  req->curr = curr;
  req->flags = REQUEST_FLAG_EAGER;
  req->length = (entry->request>>32);
  req->events = 1;

  req->remote_buffer.buf.size = req->length;
  req->remote_buffer.request = (( (uint64_t)_photon_myrank)<<32) | (entry->request<<32>>32);
  
  // reset the info ledger entry
  entry->request = 0;

  return req;
  
 error_exit:
  return NULL;
}

/* generates a new request for the recv wr
   calling this means we got an event for a corresponding post_recv()
   we know the recv mbuf entry index
   we inspected the UD hdr and determined the current sequence number
   this setup method also returns the request pointer... */
photonRequest photon_setup_request_recv(photonAddr addr, int msn, int msize, int bindex, int nbufs) {
  photonRequest req;

  req = photon_get_request(addr->global.proc_id);
  if (!req) {
    log_err("Couldn't allocate request\n");
    goto error_exit;
  }

  req->tag = -1;
  req->state = REQUEST_PENDING;
  req->type = SENDRECV;
  req->proc = addr->global.proc_id;
  req->events = nbufs;
  req->length = msize;
  //req->bentries[msn] = bindex;
  memcpy(&req->addr, addr, sizeof(*addr));
  
  //bit_array_set(req->mmask, msn);

  return req;
  
 error_exit:
  return NULL;
}

photonRequest photon_setup_request_send(photonAddr addr, int *bufs, int nbufs) {
  photonRequest req;

  req = photon_get_request(addr->global.proc_id);
  if (!req) {
    log_err("Couldn't allocate request\n");
    goto error_exit;
  }

  req->tag = -1;
  req->state = REQUEST_PENDING;
  req->type = SENDRECV;
  req->events = nbufs;
  req->length = 0;
  memcpy(&req->addr, addr, sizeof(*addr));
  //memcpy(req->bentries, bufs, sizeof(int)*DEF_MAX_BUF_ENTRIES);
  
  return req;
  
 error_exit:
  return NULL;
}
