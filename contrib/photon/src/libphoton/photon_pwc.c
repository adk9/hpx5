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

static photonRequestTable pwc_table;

int photon_pwc_init() {
  pwc_table = malloc(sizeof(struct photon_req_table_t));
  if (!pwc_table) {
    log_err("Could not allocate PWC request table");
    goto error_exit;
  }
  pwc_table->count = 0;
  pwc_table->cind = 0;
  pwc_table->tail = 0;
  pwc_table->size = roundup2pow(_photon_nproc * _LEDGER_SIZE);
  pwc_table->req_ptrs = (photonRequest*)malloc(pwc_table->size * sizeof(photonRequest));
  if (!pwc_table->req_ptrs) {
    log_err("Could not allocate request pointers for PWC table");
    goto error_exit;
  }
  memset(pwc_table->req_ptrs, 0, _LEDGER_SIZE);
 error_exit:
  return PHOTON_OK;
}

int photon_pwc_add_req(photonRequest req) {
  uint64_t req_curr, tail;
  int req_ind;
  req_curr = sync_addf(&pwc_table->count, 1, SYNC_RELAXED);
  tail = sync_load(&pwc_table->tail, SYNC_RELAXED);
  assert(tail <= req_curr);
  if ((req_curr - tail) > pwc_table->size) {
    log_err("Exceeded PWC table size: %d", pwc_table->size);
    return PHOTON_ERROR;
  }
  req_ind = req_curr & (pwc_table->size - 1);
  pwc_table->req_ptrs[req_ind] = req;
  return PHOTON_OK;
}

photonRequest photon_pwc_pop_req() {
  uint64_t req_curr, tail;
  int req_ind;
  req_curr = sync_load(&pwc_table->count, SYNC_RELAXED);
  tail = sync_load(&pwc_table->tail, SYNC_RELAXED);
  if (tail < req_curr) {
    if (sync_cas(&pwc_table->tail, tail, tail+1, SYNC_RELAXED, SYNC_RELAXED)) {
      photonRequest req;
      req_ind = (tail+1) & (pwc_table->size - 1);
      do {
	req = pwc_table->req_ptrs[req_ind];
      } while (!req);
      pwc_table->req_ptrs[req_ind] = NULL;
      return req;
    }
  }
  return NULL;
}

static int photon_pwc_try_packed(int proc, void *ptr, uint64_t size,
				 void *rptr, struct photon_buffer_priv_t priv,
				 photon_rid local, photon_rid remote, int flags,
				 int nentries) {
  // see if we should pack into an eager buffer and send in one put
  if ((size > 0) && (size <= _photon_spsize) && (size <= _photon_ebsize)) {
    photonRequest req;
    photonEagerBuf eb;
    photon_eb_hdr *hdr;
    uint64_t cookie;
    uint64_t asize;
    uintptr_t eager_addr;
    uint8_t *tail;
    int rc, offset;
    int p1_flags = 0;

    p1_flags |= (flags & PHOTON_REQ_NO_CQE)?RDMA_FLAG_NO_CQE:0;
    
    // keep offsets aligned
    asize = ALIGN(EB_MSG_SIZE(size), PWC_ALIGN);

    eb = photon_processes[proc].remote_pwc_buf;
    offset = photon_rdma_eager_buf_get_offset(proc, eb, asize,
					      ALIGN(EB_MSG_SIZE(_photon_spsize), PWC_ALIGN));
    if (offset < 0) {
      if (offset == -2) {
	return PHOTON_ERROR_RESOURCE;
      }
      goto error_exit;
    }

    eager_addr = (uintptr_t)eb->remote.addr + offset;
    
    if (nentries > 0) {
      req = photon_setup_request_direct(NULL, proc, nentries);
      if (req == NULL) {
	dbg_err("Could not setup direct buffer request");
	goto error_exit;
      }
      cookie = req->id;
      req->id = local;
      req->op = REQUEST_OP_PWC;
      req->flags = (REQUEST_FLAG_USERID | REQUEST_FLAG_1PWC);
      req->length = asize;
      req->remote_buffer.buf.addr = eager_addr;
      req->remote_buffer.buf.size = asize;
      req->remote_buffer.buf.priv = shared_storage->buf.priv;
      if (flags & PHOTON_REQ_PWC_NO_LCE) {
	req->flags |= REQUEST_FLAG_NO_LCE;
      }
    }
    else {
      cookie = NULL_COOKIE;
      req = NULL;
    }

    hdr = (photon_eb_hdr *)&(eb->data[offset]);
    hdr->header = UINT8_MAX;
    hdr->request = remote;
    hdr->addr = (uintptr_t)rptr;
    hdr->length = size;
    hdr->footer = UINT8_MAX;
    memcpy((void*)((uintptr_t)hdr + sizeof(*hdr)), ptr, size);
    // set a tail flag, the last byte in aligned buffer
    tail = (uint8_t*)((uintptr_t)hdr + asize - 1);
    *tail = UINT8_MAX;
    
    rc = __photon_backend->rdma_put(proc, (uintptr_t)hdr, (uintptr_t)eager_addr, asize,
                                    &(shared_storage->buf), &eb->remote, cookie, p1_flags);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC EAGER) failed for 0x%016lx", cookie);
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

static int photon_pwc_try_ledger(int proc, void *ptr, uint64_t size,
				 void *rptr, struct photon_buffer_priv_t priv,
				 photon_rid local, photon_rid remote, int flags, 
				 int nentries) {
  photonRequest req;
  photonBI db;
  photonLedgerEntry entry;
  uint64_t cookie;
  uintptr_t rmt_addr;
  int rc, curr = 0;
  int p0_flags = 0, p1_flags = 0;
  
  p0_flags |= ((flags & PHOTON_REQ_ONE_CQE) || (flags & PHOTON_REQ_NO_CQE))?RDMA_FLAG_NO_CQE:0;
  p1_flags |= (flags & PHOTON_REQ_NO_CQE)?RDMA_FLAG_NO_CQE:0;

  if (! (flags & PHOTON_REQ_PWC_NO_RCE)) {
    curr = photon_rdma_ledger_get_next(proc, photon_processes[proc].remote_pwc_ledger);
    if (curr < 0) {
      if (curr == -2) {
	return PHOTON_ERROR_RESOURCE;
      }
      goto error_exit;
    }
  }

  if (nentries > 0) {
    req = photon_setup_request_direct(NULL, proc, nentries);
    if (req == NULL) {
      dbg_err("Could not setup direct buffer request");
      goto error_exit;
    }
    cookie = req->id;
    req->id = local;
    req->op = REQUEST_OP_PWC;
    req->flags = (REQUEST_FLAG_USERID | REQUEST_FLAG_2PWC);
    req->length = size;
    req->remote_buffer.buf.addr = (uintptr_t)rptr;
    req->remote_buffer.buf.size = size;
    req->remote_buffer.buf.priv = priv;
    if (flags & PHOTON_REQ_PWC_NO_LCE) {
      req->flags |= REQUEST_FLAG_NO_LCE;
    }
  }
  else {
    cookie = NULL_COOKIE;
    req = NULL;
  }
  
  if (size > 0) {
    if (buffertable_find_containing( (void *)ptr, size, &db) != 0) {
      log_err("Tried posting from a buffer that's not registered");
      goto error_exit;
    }
        
    rc = __photon_backend->rdma_put(proc, (uintptr_t)ptr, (uintptr_t)rptr, size, &(db->buf),
				    &req->remote_buffer.buf, cookie, p0_flags);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC data) failed for 0x%016lx", cookie);
      goto error_exit;
    }
  }

  if (! (flags & PHOTON_REQ_PWC_NO_RCE)) {
    entry = &(photon_processes[proc].remote_pwc_ledger->entries[curr]);
    entry->request = remote;
    
    rmt_addr = (uintptr_t)photon_processes[proc].remote_pwc_ledger->remote.addr + (sizeof(*entry) * curr);        
    dbg_trace("putting into remote ledger addr: 0x%016lx", rmt_addr);
        
    rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
				    &(photon_processes[proc].remote_pwc_ledger->remote), cookie, p1_flags);
    
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC comp) failed for 0x%016lx", cookie);
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
  int rc, nentries;

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
  
  // if we didn't send any data, then we only wait on one event
  nentries = (size > 0)?2:1;
  // if we are under the small pwc eager limit, only one event
  nentries = (size <= _photon_spsize)?1:2;

  // override nentries depending on specified flags
  if ((flags & PHOTON_REQ_ONE_CQE) || (flags & PHOTON_REQ_PWC_NO_RCE)) {
    nentries = 1;
  }
  // or no events for either put
  if (flags & PHOTON_REQ_NO_CQE) {
    nentries = 0;
  }

  rc = photon_pwc_try_packed(proc, ptr, size, rptr, priv, local, remote, flags, nentries);  
  if (rc == PHOTON_ERROR_RESOURCE) {
    rc = photon_pwc_try_ledger(proc, ptr, size, rptr, priv, local, remote, flags, nentries);
  }
  
  if (rc != PHOTON_OK) {
    goto error_exit;
  }
  
  dbg_trace("Posted Request ID: %d/0x%016lx/0x%016lx", proc, local, remote);
  
  return PHOTON_OK;
  
 error_exit:
  return rc;
}

// this guy doesn't actually do any completion (yet?), just does a get and sets up a request with local rid
int _photon_get_with_completion(int proc, void *ptr, uint64_t size, void *rptr,
				struct photon_buffer_priv_t priv,
				photon_rid local, int flags) {
  photonBI db;
  photonRequest req;
  photon_rid cookie;
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

  req = photon_setup_request_direct(&rbuf, proc, 1);
  if (req == NULL) {
    dbg_trace("Could not setup direct buffer request");
    goto error_exit;
  }
  cookie = req->id;
  req->op = REQUEST_OP_PWC;
  req->id = local;
  req->remote_buffer.buf.addr = (uintptr_t)rptr;
  req->remote_buffer.buf.size = size;
  req->remote_buffer.buf.priv = priv;
  
  rc = __photon_backend->rdma_get(proc, (uintptr_t)ptr, (uintptr_t)rptr, size, &(db->buf),
				  &req->remote_buffer.buf, cookie, 0);
  if (rc != PHOTON_OK) {
    dbg_err("RDMA GET (PWC data) failed for 0x%016lx", cookie);
    goto error_exit;
  }

  dbg_trace("Posted Request ID: %d/0x%016lx", proc, local);

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
    // handle any pwc requests that were popped in some other path
    req = photon_pwc_pop_req();
    if (req != NULL) {
      assert(req->op == REQUEST_OP_PWC);
      if (! (req->flags & REQUEST_FLAG_NO_LCE)) {
	*flag = 1;
	*request = req->id;
      }
      dbg_trace("Completed and removing queued pwc request: 0x%016lx (ind=%u)",
		req->id, req->index);
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
	  *request = req->id;
	}
	dbg_trace("Completed and removing pwc request: 0x%016lx/0x%016lx (ind=%u)",
		  req->id, cookie, req->index);
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
