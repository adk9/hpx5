#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "photon_backend.h"
#include "photon_request.h"

static int __photon_cleanup_request(photonRequest req);

photonRequest photon_get_request(int proc) {
  photonRequestTable rt;
  photonRequest      req;
  uint64_t           req_curr, tail;
  uint32_t           req_ind;

  assert(proc >= 0 && proc < _photon_nproc);
  
  rt = photon_processes[proc].request_table;
  req_curr = sync_fadd(&rt->count, 1, SYNC_RELAXED);
  // offset request index by 1 since 0 is our NULL_COOKIE
  req_ind = (req_curr & (rt->size - 1)) + 1;
  tail = sync_load(&rt->tail, SYNC_RELAXED);
  assert(tail <= req_curr);
  if ((req_curr - tail) >= rt->size) {
    log_err("Request descriptors exhausted for proc %d, max=%u", proc, rt->size);
    return NULL;
  }

  req = &rt->reqs[req_ind];
  if (!req) {
    dbg_err("Uninitialized request pointer in request table");
    return NULL;
  }

  if (req->state && (req->state != REQUEST_FREE)) {
    dbg_trace("Overwriting a request (id=0x%016lx, state=%d) that never completed (curr=%lu, tail=%lu)",
	      req->id, req->state, req_curr, tail);
  }

  memset(req, 0, sizeof(struct photon_req_t));
  req->id     = PROC_REQUEST_ID(proc, req_ind);
  req->op     = REQUEST_OP_DEFAULT;
  req->state  = REQUEST_NEW;
  req->flags  = REQUEST_FLAG_NIL;
  //bit_array_clear_all(req->mmask);
  
  dbg_trace("Returning a new request (curr=%u) with id: 0x%016lx, tail=%u",
	    req_ind, req->id, tail);

  return req;
}

photonRequest photon_lookup_request(photon_rid rid) {
  photonRequestTable rt;
  uint32_t proc, id;
  id = (uint32_t)(rid<<32>>32);
  proc = (uint32_t)(rid>>32);
  if (IS_VALID_PROC(proc)) {
    rt = photon_processes[proc].request_table;
  }
  else {
    dbg_trace("Unknown proc (%u) obtained from rid: 0x%016lx", proc, rid);
    return NULL;
  }
  if (id > 0 && id <= rt->size) {
    photonRequest req = &rt->reqs[id];
    if (req->state == REQUEST_FREE) {
      dbg_warn("Looking up a request that is freed, op=%d, type=%d, id=0x%016lx",
	       req->op, req->type, req->id);
    }
    return req;
  }
  else {
    dbg_trace("Unknown request id (%u) obtained from rid: 0x%016lx", id, rid);
    return NULL;
  }
}

int photon_count_request(int proc) {
  photonRequestTable rt;
  uint64_t curr, tail;
  if (proc >= 0 && proc < _photon_nproc) {
    rt = photon_processes[proc].request_table;
    tail = sync_load(&rt->tail, SYNC_RELAXED);
    curr = sync_load(&rt->count, SYNC_RELAXED);
    return curr - tail;
  }
  else {
    return 0;
  }
}

int photon_free_request(photonRequest req) {
  photonRequestTable rt;
  int state = REQUEST_COMPLETED;
  if (!sync_cas(&req->state, state, REQUEST_FREE, SYNC_RELAXED, SYNC_RELAXED)) {
    dbg_trace("Trying to free a request that was already freed or otherwise not completed!");
    return PHOTON_ERROR;
  }
  __photon_cleanup_request(req);
  rt = photon_processes[req->proc].request_table;
  sync_fadd(&rt->tail, 1, SYNC_RELAXED);
  dbg_trace("Cleared request 0x%016lx", req->id);
  return PHOTON_OK;
}

photonRequest photon_setup_request_direct(photonBuffer lbuf, photonBuffer rbuf, uint64_t size, int proc, int events) {
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
  req->rattr.events = events;
  req->rattr.cookie = req->id;

  if (lbuf) {
    memcpy(&req->local_info.buf, lbuf, sizeof(*lbuf));
    req->local_info.id = 0;

    dbg_trace("Local info ...");
    dbg_trace("  Addr: %p", (void *)lbuf->addr);
    dbg_trace("  Size: %lu", lbuf->size);
    dbg_trace("  Keys: 0x%016lx / 0x%016lx", lbuf->priv.key0, lbuf->priv.key1);
  }
  
  if (rbuf) {
    // fill in the internal buffer with the rbuf contents
    memcpy(&req->remote_info.buf, rbuf, sizeof(*rbuf));
    // there is no matching request from the remote side, so fill in local values */
    req->remote_info.id = 0;
    
    dbg_trace("Remote info ...");
    dbg_trace("  Addr: %p", (void *)rbuf->addr);
    dbg_trace("  Size: %lu", rbuf->size);
    dbg_trace("  Keys: 0x%016lx / 0x%016lx", rbuf->priv.key0, rbuf->priv.key1);
  }
  
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

  req->state        = REQUEST_NEW;
  req->type         = EVQUEUE;
  req->proc         = proc;
  req->flags        = ri_entry->flags;
  req->size         = ri_entry->size;
  req->rattr.events = 1;

  // save the remote buffer in the request
  req->remote_info.id       = ri_entry->request;
  req->remote_info.buf.addr = ri_entry->addr;
  req->remote_info.buf.size = ri_entry->size;
  req->remote_info.buf.priv = ri_entry->priv;
  
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

  req->state        = REQUEST_NEW;
  req->type         = EVQUEUE;
  req->proc         = proc;
  req->flags        = REQUEST_FLAG_EAGER;
  req->size         = (entry->request>>32);
  req->rattr.events = 1;

  req->remote_info.buf.size = req->size;
  req->remote_info.id       = (( (uint64_t)_photon_myrank)<<32) | (entry->request<<32>>32);
  
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

  req->tag          = PHOTON_ANY_TAG;
  req->state        = REQUEST_PENDING;
  req->type         = SENDRECV;
  req->proc         = addr->global.proc_id;
  req->size         = msize;
  req->rattr.events = nbufs;
  //req->bentries[msn] = bindex;
  //memcpy(&req->addr, addr, sizeof(*addr));
  
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

  req->tag          = PHOTON_ANY_TAG;
  req->state        = REQUEST_PENDING;
  req->type         = SENDRECV;
  req->size         = 0;
  req->rattr.events = nbufs;
  //memcpy(&req->addr, addr, sizeof(*addr));
  //memcpy(req->bentries, bufs, sizeof(int)*DEF_MAX_BUF_ENTRIES);
  
  return req;
  
 error_exit:
  return NULL;
}

static int __photon_cleanup_request(photonRequest req) {
  switch (req->op) {
  case REQUEST_OP_SENDBUF:
    if (req->flags & REQUEST_FLAG_EAGER) {
      MARK_DONE(photon_processes[req->proc].remote_eager_buf, req->size);
      MARK_DONE(photon_processes[req->proc].remote_eager_ledger, 1);
    }
    else {
      MARK_DONE(photon_processes[req->proc].remote_snd_info_ledger, 1);
    }
    break;
  case REQUEST_OP_SENDREQ:
    MARK_DONE(photon_processes[req->proc].remote_snd_info_ledger, 1);
    break;
  case REQUEST_OP_SENDFIN:
    break;
  case REQUEST_OP_RECVBUF:
    MARK_DONE(photon_processes[req->proc].remote_rcv_info_ledger, 1);
    break;
  case REQUEST_OP_PWC:
    if (req->flags & REQUEST_FLAG_1PWC) {
      MARK_DONE(photon_processes[req->proc].remote_pwc_buf, req->size);
    }
    else if (req->flags & REQUEST_FLAG_2PWC) {
      MARK_DONE(photon_processes[req->proc].remote_pwc_ledger, 1);
    }
    break;
  case REQUEST_OP_DEFAULT:
    break;
  default:
    log_err("Tried to cleanup a request op we don't recognize: %d", req->op);
    break;
  }

  return PHOTON_OK;
}
