#include <stdlib.h>
#include <string.h>

#include "photon_backend.h"
#include "photon_request.h"

photonRequest __photon_get_request() {
  photonRequest req;

  LIST_LOCK(&free_reqs_list);
  req = LIST_FIRST(&free_reqs_list);
  if (req)
    LIST_REMOVE(req, list);
  LIST_UNLOCK(&free_reqs_list);
  
  if (!req) {
    dbg_trace("Request list is empty, this should rarely happen");
    req = malloc(sizeof(struct photon_req_t));
    req->mmask = bit_array_create(UD_MASK_SIZE);
  }

  req->flags = REQUEST_FLAG_NIL;
  req->num_entries = 0;
  bit_array_clear_all(req->mmask);

  return req;
}

/* request id has already been set  */
int __photon_setup_request_direct(photonBuffer rbuf, int proc, int flags, int entries, photon_rid rid, photon_rid eid) {
  photonRequest req;
  
  req = __photon_get_request();
  if (!req) {
    log_err("Couldn't allocate request");
    goto error_exit;
  }

  req->id = rid;
  req->state = REQUEST_PENDING;
  req->type = EVQUEUE;
  req->proc = proc;
  req->tag = 0;
  req->length = rbuf->size;
  req->num_entries = entries;
  req->flags = flags;
  
  /* fill in the internal buffer with the rbuf contents */
  memcpy(&req->remote_buffer, rbuf, sizeof(*rbuf));
  /* there is no matching request from the remote side, so fill in local values */
  req->remote_buffer.request = rid;
  req->remote_buffer.tag = req->tag;
  
  dbg_trace("Remote request: 0x%016lx", rid);
  dbg_trace("Addr: %p", (void *)rbuf->addr);
  dbg_trace("Size: %lu", rbuf->size);
  dbg_trace("Flags: %d",	flags);
  dbg_trace("Keys: 0x%016lx / 0x%016lx", rbuf->priv.key0, rbuf->priv.key1);

  if (flags & REQUEST_FLAG_USERID) {
    dbg_trace("Inserting the OS put request into the pwc request table: %d/0x%016lx/%p", proc, eid, req);
    if (htable_insert(pwc_reqtable, eid, req) != 0) {
      log_err("Couldn't save request in hashtable");
    }
  }
  else {
    dbg_trace("Inserting the OS put request into the request table: %d/0x%016lx/%p", proc, eid, req);
    if (htable_insert(reqtable, eid, req) != 0) {
      log_err("Couldn't save request in hashtable");
    }
  }

  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

/* generates a new request for the received info ledger event */
int __photon_setup_request_ledger_info(photonRILedgerEntry ri_entry, int curr, int proc, photon_rid *request) {
  photonRequest req;
  photon_rid request_id;

  request_id = (( (uint64_t)proc)<<32) | sync_fadd(&req_counter, 1, SYNC_RELEASE);
  dbg_trace("Incrementing curr_cookie_count to: 0x%016lx", request_id);

  *request = request_id;
  
  req = __photon_get_request();
  if (!req) {
    log_err("Couldn't allocate request\n");
    goto error_exit;
  }
  req->id = request_id;
  req->state = REQUEST_NEW;
  req->type = EVQUEUE;
  req->proc = proc;
  req->curr = curr;
  req->flags = ri_entry->flags;
  req->length = ri_entry->size;

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

  dbg_trace("Inserting the new send buffer request into the request table: %d/0x%016lx/%p", proc, request_id, req);
  if (htable_insert(reqtable, request_id, req) != 0) {
    /* this is bad, we've submitted the request, but we can't track it */
    log_err("Couldn't save request in hashtable");
    goto error_exit;
  }

  // reset the info ledger entry
  ri_entry->header = 0;
  ri_entry->footer = 0;
  
  return PHOTON_OK;
  
 error_exit:
  if (request != NULL) {
    *request = NULL_COOKIE;
  }
  return PHOTON_ERROR;
}

int __photon_setup_request_ledger_eager(photonLedgerEntry entry, int curr, int proc, photon_rid *request) {
  photonRequest req;
  photon_rid request_id;

  request_id = (( (uint64_t)proc)<<32) | sync_fadd(&req_counter, 1, SYNC_RELEASE);
  dbg_trace("Incrementing curr_cookie_count to: 0x%016lx", request_id);

  *request = request_id;
  
  req = __photon_get_request();
  if (!req) {
    log_err("Couldn't allocate request\n");
    goto error_exit;
  }
  req->id = request_id;
  req->state = REQUEST_NEW;
  req->type = EVQUEUE;
  req->proc = proc;
  req->curr = curr;
  req->flags = REQUEST_FLAG_EAGER;
  req->length = (entry->request>>32);

  req->remote_buffer.buf.size = req->length;
  req->remote_buffer.request = (( (uint64_t)_photon_myrank)<<32) | (entry->request<<32>>32);
  
  dbg_trace("Inserting the new eager buffer request into the request table: %d/0x%016lx/%p", proc, request_id, req);
  if (htable_insert(reqtable, request_id, req) != 0) {
    /* this is bad, we've submitted the request, but we can't track it */
    log_err("Couldn't save request in hashtable");
    goto error_exit;
  }

  // reset the info ledger entry
  entry->request = 0;

  return PHOTON_OK;
  
 error_exit:
  if (request != NULL) {
    *request = NULL_COOKIE;
  }
  return PHOTON_ERROR;
}

/* generates a new request for the recv wr
   calling this means we got an event for a corresponding post_recv()
   we know the recv mbuf entry index
   we inspected the UD hdr and determined the current sequence number
   this setup method also returns the request pointer... */
photonRequest __photon_setup_request_recv(photonAddr addr, int msn, int msize, int bindex, int nbufs, photon_rid request) {
  photonRequest req;

  req = __photon_get_request();
  if (!req) {
    log_err("Couldn't allocate request\n");
    goto error_exit;
  }

  req->id = request;
  req->tag = -1;
  req->state = REQUEST_PENDING;
  req->type = SENDRECV;
  req->proc = addr->global.proc_id;
  req->num_entries = nbufs;
  req->length = msize;
  req->bentries[msn] = bindex;
  memcpy(&req->addr, addr, sizeof(*addr));
  
  bit_array_set(req->mmask, msn);

  dbg_trace("Inserting the new recv request into the sr table: %lu/0x%016lx/%p",
           addr->global.proc_id, request, req);
  if (htable_insert(sr_reqtable, request, req) != 0) {
    /* this is bad, we've submitted the request, but we can't track it */
    log_err("Couldn't save request in hashtable");
    goto error_exit;
  }

  return req;
  
 error_exit:
  return NULL;
}

/* generates a new request for the send wr */
int __photon_setup_request_send(photonAddr addr, int *bufs, int nbufs, photon_rid request) {
  photonRequest req;

  req = __photon_get_request();
  if (!req) {
    log_err("Couldn't allocate request\n");
    goto error_exit;
  }

  req->id = request;
  req->tag = -1;
  req->state = REQUEST_PENDING;
  req->type = SENDRECV;
  req->num_entries = nbufs;
  req->length = 0;
  memcpy(&req->addr, addr, sizeof(*addr));
  memcpy(req->bentries, bufs, sizeof(int)*DEF_MAX_BUF_ENTRIES);
  
  dbg_trace("Inserting the new send request into the sr table: 0x%016lx/0x%016lx/%p",
           addr->global.proc_id, request, req);
  if (htable_insert(sr_reqtable, request, req) != 0) {
    /* this is bad, we've submitted the request, but we can't track it */
    log_err("Couldn't save request in hashtable");
    goto error_exit;
  }

  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}
