#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include "photon_backend.h"
#include "photon_buffertable.h"
#include "photon_event.h"
#include "photon_pwc.h"
#include "util.h"

two_lock_queue_t          *pwc_q;

int photon_pwc_init() {
  pwc_q = sync_two_lock_queue_new();
  if (!pwc_q)
    goto error_exit;
  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

int photon_pwc_add_req(photonRequest req) {
  sync_two_lock_queue_enqueue(pwc_q, req);
  return PHOTON_OK;
}

photonRequest photon_pwc_pop_req() {
  return sync_two_lock_queue_dequeue(pwc_q);
}

static int photon_pwc_try_packed(photonRequest req) {
  // see if we should pack into an eager buffer and send in one put
  if ((req->size > 0) && (req->size <= _photon_spsize) &&
      (req->size <= _photon_ebsize)) {
    photonEagerBuf eb;
    photon_eb_hdr *hdr;
    uint64_t asize;
    uintptr_t eager_addr;
    uint8_t *tail;
    int rc, offset;

    // keep offsets aligned
    asize = ALIGN(EB_MSG_SIZE(req->size), PWC_ALIGN);

    eb = photon_processes[req->proc].remote_pwc_buf;
    offset = photon_rdma_eager_buf_get_offset(req->proc, eb, asize,
					      ALIGN(EB_MSG_SIZE(_photon_spsize), PWC_ALIGN));
    if (offset < 0) {
      return PHOTON_ERROR_RESOURCE;
    }
    
    req->flags        |= REQUEST_FLAG_1PWC;
    req->rattr.events  = 1;
    
    eager_addr = (uintptr_t)eb->remote.addr + offset;
    hdr = (photon_eb_hdr *)&(eb->data[offset]);
    hdr->header  = UINT8_MAX;
    hdr->request = req->remote_info.id;
    hdr->addr    = req->remote_info.buf.addr;
    hdr->length  = req->size;
    hdr->footer  = UINT8_MAX;

    memcpy((void*)((uintptr_t)hdr + sizeof(*hdr)), (void*)req->local_info.buf.addr, req->size);
    // set a tail flag, the last byte in aligned buffer
    tail = (uint8_t*)((uintptr_t)hdr + asize - 1);
    *tail = UINT8_MAX;
    
    rc = __photon_backend->rdma_put(req->proc, (uintptr_t)hdr, (uintptr_t)eager_addr, asize,
                                    &(shared_storage->buf), &eb->remote, req->rattr.cookie,
				    RDMA_FLAG_NIL);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC EAGER) failed for 0x%016lx", req->rattr.cookie);
      goto error_exit;
    }
  } 
  else {
    // size is too large, try something else
    return PHOTON_ERROR_RESOURCE;
  }

  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}

static int photon_pwc_try_ledger(photonRequest req) {
  photonBI db;
  photonLedger l;
  photonLedgerEntry entry;
  uintptr_t rmt_addr;
  int rc, curr = 0;

  req->flags |= REQUEST_FLAG_2PWC;
  req->rattr.events = 1;

  l = photon_processes[req->proc].remote_pwc_ledger;
  if (! (req->flags & REQUEST_FLAG_NO_RCE)) {
    curr = photon_rdma_ledger_get_next(req->proc, l);
    if (curr < 0) {
      return PHOTON_ERROR_RESOURCE;
    }
  }
  
  if (req->size > 0) {
    if (buffertable_find_containing( (void *)req->local_info.buf.addr, req->size, &db) != 0) {
      log_err("Tried posting from a buffer that's not registered");
      goto error_exit;
    }
    
    req->rattr.events = 2;

    rc = __photon_backend->rdma_put(req->proc, req->local_info.buf.addr,
				    req->remote_info.buf.addr, req->size, &(db->buf),
				    &req->remote_info.buf, req->rattr.cookie,
				    RDMA_FLAG_NIL);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC data) failed for 0x%016lx", req->rattr.cookie);
      goto error_exit;
    }
  }
  
  if (! (req->flags & REQUEST_FLAG_NO_RCE)) {
    entry = &(l->entries[curr]);
    entry->request = req->remote_info.id;
    
    rmt_addr = (uintptr_t)l->remote.addr + (sizeof(*entry) * curr);
    dbg_trace("putting into remote ledger addr: 0x%016lx", rmt_addr);
    
    rc = __photon_backend->rdma_put(req->proc, (uintptr_t)entry, rmt_addr,
				    sizeof(*entry), &(shared_storage->buf),
				    &(l->remote), req->rattr.cookie,
				    RDMA_FLAG_NIL);
    
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC comp) failed for 0x%016lx", req->rattr.cookie);
      goto error_exit;
    }
  }
  
  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}
  
int _photon_put_with_completion(int proc, void *ptr, uint64_t size, void *rptr,
				       struct photon_buffer_priv_t priv,
                                       photon_rid local, photon_rid remote, int flags) {
  photonRequest req;
  int rc;
  static int count = 0;

  dbg_trace("(%d, %p, %lu, %p, 0x%016lx, 0x%016lx)", proc, ptr, size, rptr, local, remote);

  rc = PHOTON_ERROR;

  if (size && !ptr) {
    log_err("Trying to put size %lu and NULL ptr", size);
    goto error_exit;
  }
  
  if (size && !rptr) {
    log_err("Tring to put size %lu and NULL rptr", size);
    goto error_exit;
  }

  if (!size && (flags & PHOTON_REQ_PWC_NO_RCE)) {
    dbg_warn("Nothing to send and no remote completion requested!");
    return PHOTON_OK;
  }

  req = photon_get_request(proc);
  if (!req) {
    dbg_err("Could not allocate request");
    goto error_exit;
  }

  req->proc  = proc;
  req->flags = REQUEST_FLAG_NIL;
  req->op    = REQUEST_OP_PWC;
  req->type  = EVQUEUE;
  req->state = REQUEST_PENDING;
  req->size  = size;
  
  req->local_info.id        = local;
  req->local_info.buf.addr  = (uintptr_t)ptr;
  req->local_info.buf.size  = size;
  // local buffer should have been registered to photon
  // but we only need buffer metadata if doing 2-put

  req->remote_info.id       = remote;
  req->remote_info.buf.addr = (uintptr_t)rptr;
  req->remote_info.buf.size = size;
  req->remote_info.buf.priv = priv;

  // control the return of the local id
  if (flags & PHOTON_REQ_PWC_NO_LCE) {
    req->flags |= REQUEST_FLAG_NO_LCE;
  }

  // control the return of the remote id
  if (flags & PHOTON_REQ_PWC_NO_RCE) {
    req->flags |= REQUEST_FLAG_NO_RCE;
  }

  // set a cookie for the completion events
  req->rattr.cookie = req->id;
  
  rc = photon_pwc_try_packed(req);
  if (rc == PHOTON_ERROR_RESOURCE) {
    rc = photon_pwc_try_ledger(req);
  }
  
  if (rc != PHOTON_OK) {
    sync_two_lock_queue_enqueue(photon_processes[proc].request_table->req_q, req);
    //dbg_info("enqueue count: %d", ++count);
    goto error_exit;
  }
  
  dbg_trace("Posted Request ID: %d/0x%016lx/0x%016lx", proc, local, remote);
  
  return PHOTON_OK;
  
 error_exit:
  return rc;
}

int _photon_get_with_completion(int proc, void *ptr, uint64_t size, void *rptr,
				struct photon_buffer_priv_t priv,
				photon_rid local, int flags) {
  photonBI db;
  photonRequest req;
  struct photon_buffer_t lbuf;
  struct photon_buffer_t rbuf;
  int rc;

  dbg_trace("(%d, %p, %lu, %p, 0x%016lx)", proc, ptr, size, rptr, local);

  if (size && !rptr) {
    log_err("Tring to get size %lu and NULL rptr", size);
    goto error_exit;
  }

  if (!size || !ptr) {
    log_err("Trying to get 0 bytes or into NULL ptr");
    goto error_exit;
  }

  if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
    log_err("Tried posting from a buffer that's not registered");
    goto error_exit;
  }

  lbuf.addr = (uintptr_t)ptr;
  lbuf.size = size;
  lbuf.priv = db->buf.priv;

  rbuf.addr = (uintptr_t)rptr;
  rbuf.size = size;
  rbuf.priv = priv;

  req = photon_setup_request_direct(&lbuf, &rbuf, size, proc, 1);
  if (req == NULL) {
    dbg_trace("Could not setup direct buffer request");
    goto error_exit;
  }
  
  req->op = REQUEST_OP_PWC;
  req->local_info.id = local;

  rc = __photon_backend->rdma_get(proc, (uintptr_t)ptr, (uintptr_t)rptr, size, &(db->buf),
				  &req->remote_info.buf, req->rattr.cookie, RDMA_FLAG_NIL);
  if (rc != PHOTON_OK) {
    dbg_err("RDMA GET (PWC data) failed for 0x%016lx", req->rattr.cookie);
    goto error_exit;
  }

  dbg_trace("Posted Request ID: %d/0x%016lx/0x%016lx", proc, local, req->rattr.cookie);

  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

int _photon_probe_completion(int proc, int *flag, photon_rid *request, int flags) {
  photonLedger ledger;
  photonLedgerEntry entry_iter;
  photonRequest req;
  photonEagerBuf eb;
  photon_eb_hdr *hdr;
  photon_rid cookie = NULL_COOKIE;
  int i, rc, start, end;

  *flag = 0;

  if (proc == PHOTON_ANY_SOURCE) {
    start = 0;
    end = _photon_nproc;
  }
  else {
    start = proc;
    end = proc+1;
  }

  if (flags & PHOTON_PROBE_EVQ) {
    // process any queued PWC requests
    for (i=start; i<end; i++) {
      req = sync_two_lock_queue_dequeue(photon_processes[i].request_table->req_q);
      if (req) {
	rc = photon_pwc_try_ledger(req);
	if (rc != PHOTON_OK) {
	  sync_two_lock_queue_enqueue(photon_processes[i].request_table->req_q, req);
	}
      }
    }

    // handle any pwc requests that were popped in some other path
    req = photon_pwc_pop_req();
    if (req != NULL) {
      assert(req->op == REQUEST_OP_PWC);
      assert(req->state == REQUEST_COMPLETED);
      if (! (req->flags & REQUEST_FLAG_NO_LCE)) {
	*flag = 1;
	*request = req->local_info.id;
      }
      dbg_trace("Completed and removing queued pwc request: 0x%016lx (ind=0x%016lx)",
		req->id, req->local_info.id);
      photon_free_request(req);
      return PHOTON_OK;
    }

    rc = __photon_get_event(&cookie);
    if (rc == PHOTON_EVENT_ERROR) {
      dbg_err("Error getting event, rc=%d", rc);
      goto error_exit;
    }
    else if (rc == PHOTON_EVENT_NONE) {
      cookie = NULL_COOKIE;
    }
    else {
      // we found an event to process
      int rc;
      rc = __photon_handle_cq_event(NULL, cookie, &req);
      if (rc == PHOTON_EVENT_ERROR) {
	goto error_exit;
      }
      else if ((rc == PHOTON_EVENT_REQCOMP) && req &&
	       (req->op == REQUEST_OP_PWC)) {
	// sometimes the requestor doesn't care about the completion
	if (! (req->flags & REQUEST_FLAG_NO_LCE)) {
	  *flag = 1;
	  *request = req->local_info.id;
	}
	dbg_trace("Completed and removing pwc request: 0x%016lx (id=0x%016lx)",
		  req->id, req->local_info.id);
	photon_free_request(req);
	return PHOTON_OK;
      }
      else {
	dbg_trace("PWC probe handled non-completion event: 0x%016lx", cookie);
      }
    }
  }
  
  // only check recv ledgers if an event we don't care about was popped
  if ((cookie == NULL_COOKIE) && (flags & PHOTON_PROBE_LEDGER)) {
    uint64_t offset, curr, new, left;
    for (i=start; i<end; i++) {
      // check eager region first
      eb = photon_processes[i].local_pwc_buf;
      curr = sync_load(&eb->curr, SYNC_RELAXED);
      offset = curr & (eb->size - 1);
      left = eb->size - offset;
      if (left < ALIGN(EB_MSG_SIZE(_photon_spsize), PWC_ALIGN)) {
	new = left + curr;
	offset = 0;
      }
      else {
	new = curr;
      }

      hdr = (photon_eb_hdr *)&(eb->data[offset]);
      if ((hdr->header == UINT8_MAX) && (hdr->footer == UINT8_MAX)) {
	photon_rid req = hdr->request;
	uintptr_t addr = hdr->addr;
	uint16_t size = hdr->length;
	uint64_t asize = ALIGN(EB_MSG_SIZE(size), PWC_ALIGN);
	if (sync_cas(&eb->curr, curr, new+asize, SYNC_RELAXED, SYNC_RELAXED)) {
	  // now check for tail flag (or we could return to check later)
	  volatile uint8_t *tail = (uint8_t*)((uintptr_t)hdr + asize - 1);
	  while (*tail != UINT8_MAX)
	    ;
	  memcpy((void*)addr, (void*)((uintptr_t)hdr + sizeof(*hdr)), size);
	  *request = req;
	  *flag = 1;
	  dbg_trace("Copied message of size %u into 0x%016lx for request 0x%016lx",
		   size, addr, req);
	  memset((void*)hdr, 0, asize);
	  sync_store(&eb->prog, new+asize, SYNC_RELAXED);
	  return PHOTON_OK;
	}
      }
      
      // then check pwc ledger
      ledger = photon_processes[i].local_pwc_ledger;
      curr = sync_load(&ledger->curr, SYNC_RELAXED);
      offset = curr & (ledger->num_entries - 1);
      entry_iter = &(ledger->entries[offset]);
      if (entry_iter->request != (photon_rid) UINT64_MAX &&
	  sync_cas(&ledger->curr, curr, curr+1, SYNC_RELAXED, SYNC_RELAXED)) {
	*request = entry_iter->request;
	entry_iter->request = UINT64_MAX;
	*flag = 1;
	sync_fadd(&ledger->prog, 1, SYNC_RELAXED);
	dbg_trace("Popped ledger event with id: 0x%016lx (%lu)", *request, *request);
	return PHOTON_OK;
      }
    }
  }
    
  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}
