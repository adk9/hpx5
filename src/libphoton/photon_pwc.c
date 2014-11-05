#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "photon_backend.h"
#include "photon_buffertable.h"
#include "photon_pwc.h"

int _photon_put_with_completion(int proc, void *ptr, uint64_t size, void *rptr,
				       struct photon_buffer_priv_t priv,
                                       photon_rid local, photon_rid remote, int flags) {
  photonBI db;
  photonRequest req;
  photonLedgerEntry entry;
  photon_rid cookie;
  uintptr_t rmt_addr;
  int rc, curr, nentries;
  int p0_flags = 0, p1_flags = 0;

  dbg_trace("(%d, %p, %lu, %p, 0x%016lx, 0x%016lx)", proc, ptr, size, rptr, local, remote);

  p0_flags |= ((flags & PHOTON_REQ_ONE_CQE) || (flags & PHOTON_REQ_NO_CQE))?RDMA_FLAG_NO_CQE:0;
  p1_flags |= (flags & PHOTON_REQ_NO_CQE)?RDMA_FLAG_NO_CQE:0;
  
  // if we didn't send any data, then we only wait on one event
  nentries = (size > 0)?2:1;
  // if we are under the small pwc eager limit, only one event
  nentries = (size <= _photon_spsize)?1:2;

  // check if we only get event for one put
  if (nentries == 2 && (flags & PHOTON_REQ_ONE_CQE))
    nentries = 1;
  // or no events for either put
  if (flags & PHOTON_REQ_NO_CQE)
    nentries = 0;
  
  if (nentries > 0) {
    req = photon_setup_request_direct(NULL, proc, nentries);
    if (req == NULL) {
      dbg_trace("Could not setup direct buffer request");
      goto error_exit;
    }
    cookie = req->id;
    req->id = local;
    req->flags = PHOTON_REQ_USERID;
  }
  else {
    cookie = NULL_COOKIE;
  }

  // see if we should pack into an eager buffer and send in one put
  if ((size > 0) && (size <= _photon_spsize) && (size <= _photon_ebsize)) {
    photonEagerBuf eb;
    photon_eb_hdr *hdr;
    uintptr_t eager_addr;
    uint8_t *tail;
    int offset;
    int tadd = 0;

    eb = photon_processes[proc].remote_pwc_buf;
    offset = photon_rdma_eager_buf_get_offset(eb, EB_MSG_SIZE(size), EB_MSG_SIZE(_photon_spsize));
    if (offset < 0) {
      goto error_exit;
    }
    hdr = (photon_eb_hdr *)&(eb->data[offset]);
    hdr->request = remote;
    hdr->addr = (uintptr_t)rptr;
    hdr->length = size;
    hdr->head = UINT8_MAX;
    memcpy((void*)((uintptr_t)hdr + sizeof(*hdr)), ptr, size);
    // set a tail flag
    tail = (uint8_t*)((uintptr_t)hdr + sizeof(*hdr) + size);
    // align to correct boundary
    tadd = ((uintptr_t)tail % PWC_ALIGN != 0)?(PWC_ALIGN - (uintptr_t)tail % PWC_ALIGN):0;
    tail += tadd;
    *tail = UINT8_MAX;

    eager_addr = (uintptr_t)eb->remote.addr + offset;
    req->remote_buffer.buf.addr = eager_addr;
    req->remote_buffer.buf.size = EB_MSG_SIZE(size);
    req->remote_buffer.buf.priv = shared_storage->buf.priv;
    req->length = EB_MSG_SIZE(size);
    req->flags |= REQUEST_FLAG_1PWC;

    rc = __photon_backend->rdma_put(proc, (uintptr_t)hdr, (uintptr_t)eager_addr, EB_MSG_SIZE(size),
                                    &(shared_storage->buf), &eb->remote, cookie, p1_flags);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC EAGER) failed for 0x%016lx", cookie);
      goto error_exit;
    }
  }
  // do the unpacked 2-put version instead
  else { 
    curr = photon_rdma_ledger_get_next(photon_processes[proc].remote_pwc_ledger);
    if (curr < 0) {
      goto error_exit;
    }

    if (size > 0) {
      if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
        log_err("Tried posting from a buffer that's not registered");
        goto error_exit;
      }
      
      req->remote_buffer.buf.addr = (uintptr_t)rptr;
      req->remote_buffer.buf.size = size;
      req->remote_buffer.buf.priv = priv;

      rc = __photon_backend->rdma_put(proc, (uintptr_t)ptr, (uintptr_t)rptr, size, &(db->buf),
                                      &req->remote_buffer.buf, cookie, p0_flags);
      if (rc != PHOTON_OK) {
        dbg_err("RDMA PUT (PWC data) failed for 0x%016lx", cookie);
        goto error_exit;
      }
    }

    entry = &(photon_processes[proc].remote_pwc_ledger->entries[curr]);
    entry->request = remote;

    rmt_addr = (uintptr_t)photon_processes[proc].remote_pwc_ledger->remote.addr + (sizeof(*entry) * curr);        
    dbg_trace("putting into remote ledger addr: 0x%016lx", rmt_addr);

    req->length = size;
    req->flags |= REQUEST_FLAG_2PWC;
    
    rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
                                    &(photon_processes[proc].remote_pwc_ledger->remote), cookie, p1_flags);
    
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC comp) failed for 0x%016lx", cookie);
      goto error_exit;
    }
  }
  
  dbg_trace("Posted Request ID: %d/0x%016lx/0x%016lx", proc, local, remote);
  
  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;  
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

  if (!size || !ptr) {
    log_err("GET (PWC) trying to GET 0 bytes or into NULL buf");
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
  req->id = local;
  req->flags = REQUEST_FLAG_USERID;
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
  photon_event_status event;
  photon_eb_hdr *hdr;
  photon_rid cookie = UINT64_MAX;
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
    rc = __photon_backend->get_event(&event);
    if (rc < 0) {
      dbg_err("Error getting event, rc=%d", rc);
      goto error_exit;
    }
    if (rc == PHOTON_OK) {
      cookie = event.id;
      dbg_trace("popped CQ event with id: 0x%016lx", cookie);
    }
  }

  // prioritize the EVQ
  if ((flags & PHOTON_PROBE_LEDGER) && (cookie == UINT64_MAX)) {
    uint64_t offset, curr, new, left;
    for (i=start; i<end; i++) {
      // check eager region first
      eb = photon_processes[i].local_pwc_buf;
      curr = sync_load(&eb->curr, SYNC_RELAXED);
      offset = curr % eb->size;
      left = eb->size - offset;
      if (left < EB_MSG_SIZE(_photon_spsize)) {
	new = left + curr;
	offset = 0;
      }
      else {
	new = curr;
      }
      hdr = (photon_eb_hdr *)&(eb->data[offset]);
      if (hdr->head == UINT8_MAX) {
	uint16_t size = hdr->length;
	photon_rid req = hdr->request;
	uintptr_t addr = hdr->addr;
	new += EB_MSG_SIZE(size);
	if (sync_cas(&eb->curr, curr, new, SYNC_RELAXED, SYNC_RELAXED)) {
	  // now check for tail flag (or we could return to check later)
	  volatile uint8_t *tail = (uint8_t*)((uintptr_t)hdr + sizeof(*hdr) + size);
	  int tadd = ((uintptr_t)tail % PWC_ALIGN != 0)?(PWC_ALIGN - (uintptr_t)tail % PWC_ALIGN):0;
	  tail += tadd;
	  while (*tail != UINT8_MAX)
	    ;
	  memcpy((void*)addr, (void*)((uintptr_t)hdr + sizeof(*hdr)), size);
	  *request = req;
	  *flag = 1;
	  dbg_trace("Copied message of size %u into 0x%016lx for request 0x%016lx",
		    size, addr, req);
	  memset((void*)hdr, 0, EB_MSG_SIZE(size));
	  return PHOTON_OK;
	}
      }
      
      // then check pwc ledger
      ledger = photon_processes[i].local_pwc_ledger;
      curr = sync_load(&ledger->curr, SYNC_RELAXED);
      offset = curr % ledger->num_entries;
      entry_iter = &(ledger->entries[offset]);
      if (entry_iter->request != (photon_rid) UINT64_MAX && 
	  sync_cas(&ledger->curr, curr, curr+1, SYNC_RELAXED, SYNC_RELAXED)) {
	  *request = entry_iter->request;
	  *flag = 1;
	  entry_iter->request = UINT64_MAX;
	  dbg_trace("Popped ledger event with id: 0x%016lx", *request);
	  return PHOTON_OK;
      }
    }
  }
  
  // we found something to process
  if (cookie != UINT64_MAX) {
    req = photon_lookup_request(cookie);
    if (req) {
      // set flag and request only if we have processed the number of outstanding
      // events expected for this reqeust
      if ( (--req->events) == 0) {
	*flag = 1;
	*request = req->id;
	// accounting
	if (req->flags & REQUEST_FLAG_1PWC) {
	  MARK_DONE(photon_processes[req->proc].remote_pwc_buf, req->length);
	}
	else if (req->flags & REQUEST_FLAG_2PWC) {
	  MARK_DONE(photon_processes[req->proc].remote_pwc_ledger, 1);
	}
	dbg_trace("Completed and removing pwc request: 0x%016lx", cookie);
	photon_free_request(req);
	return PHOTON_OK;
      }
    }
    else {
      dbg_trace("Got a CQ event not tracked: 0x%016lx", cookie);
      goto error_exit;
    }
  }
  
  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}
