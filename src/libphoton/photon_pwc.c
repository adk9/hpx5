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
  photonLedgerEntry entry;
  photon_rid request_id;
  struct photon_buffer_t rbuf;
  uintptr_t rmt_addr;
  int rc, curr, nentries;
  int p0_flags = 0, p1_flags = 0;

  dbg_trace("(%d, %p, %lu, %p, 0x%016lx, 0x%016lx)", proc, ptr, size, rptr, local, remote);

  request_id = (( (uint64_t)proc)<<32) | INC_COUNTER(curr_cookie);
  
  p0_flags |= ((flags & PHOTON_REQ_ONE_CQE) || (flags & PHOTON_REQ_NO_CQE))?RDMA_FLAG_NO_CQE:0;
  p1_flags |= (flags & PHOTON_REQ_NO_CQE)?RDMA_FLAG_NO_CQE:0;
  
  // if we didn't send any data, then we only wait on one event
  nentries = (size > 0)?2:1;
  // if we are under the small pwc eager limit, only one event
  nentries = (size <= __photon_config->cap.small_pwc_size)?1:2;

  // check if we only get event for one put
  if (nentries == 2 && (flags & PHOTON_REQ_ONE_CQE))
    nentries = 1;
  // or no events for either put
  if (flags & PHOTON_REQ_NO_CQE)
    nentries = 0;
  
  if (nentries > 0) {
    rc = __photon_setup_request_direct(&rbuf, proc, REQUEST_FLAG_USERID, nentries, local, request_id);
    if (rc != PHOTON_OK) {
      dbg_trace("Could not setup direct buffer request");
      goto error_exit;
    }
  }

  // see if we should pack into an eager buffer and send in one put
  if ((size > 0) && (size <= __photon_config->cap.small_pwc_size) && (size <= _photon_ebsize)) {
    photonEagerBuf eb;
    photon_eb_hdr *hdr;
    uintptr_t eager_addr;
    uint8_t *tail;
    int tadd = 0;

    eb = photon_processes[proc].remote_pwc_buf;
    hdr = (photon_eb_hdr *)&(eb->data[eb->offset]);
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

    eager_addr = (uintptr_t)eb->remote.addr + eb->offset;
    rbuf.addr = eager_addr;
    rbuf.size = EB_MSG_SIZE(size+tadd);
    rbuf.priv = shared_storage->buf.priv;

    rc = __photon_backend->rdma_put(proc, (uintptr_t)hdr, (uintptr_t)eager_addr, EB_MSG_SIZE(size+tadd),
                                    &(shared_storage->buf), &eb->remote, request_id, p1_flags);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC EAGER) failed for 0x%016lx", request_id);
      goto error_exit;
    }

    NEXT_EAGER_BUF(eb, EB_MSG_SIZE(size+tadd));
    if ((_photon_ebsize - eb->offset) < EB_MSG_SIZE(__photon_config->cap.small_pwc_size))
      eb->offset = 0;
  }
  // do the unpacked 2-put version instead
  else { 
    if (size > 0) {
      if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
        log_err("Tried posting from a buffer that's not registered");
        goto error_exit;
      }
      
      rbuf.addr = (uintptr_t)rptr;
      rbuf.size = size;
      rbuf.priv = priv;
      
      rc = __photon_backend->rdma_put(proc, (uintptr_t)ptr, (uintptr_t)rptr, size, &(db->buf),
                                      &rbuf, request_id, p0_flags);
      if (rc != PHOTON_OK) {
        dbg_err("RDMA PUT (PWC data) failed for 0x%016lx", request_id);
        goto error_exit;
      }
    }
    
    curr = photon_processes[proc].remote_pwc_ledger->curr;
    entry = &(photon_processes[proc].remote_pwc_ledger->entries[curr]);
    rmt_addr = (uintptr_t)photon_processes[proc].remote_pwc_ledger->remote.addr + (sizeof(*entry) * curr);
    
    entry->request = remote;
    
    dbg_trace("putting into remote ledger addr: 0x%016lx", rmt_addr);
    
    rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
                                    &(photon_processes[proc].remote_pwc_ledger->remote), request_id, p1_flags);
    
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC comp) failed for 0x%016lx", request_id);
      goto error_exit;
    }
    NEXT_LEDGER_ENTRY(photon_processes[proc].remote_pwc_ledger);
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
  photon_rid request_id;
  struct photon_buffer_t rbuf;
  int rc;

  dbg_trace("(%d, %p, %lu, %p, 0x%016lx)", proc, ptr, size, rptr, local);

  if (!size || !ptr) {
    log_err("GET (PWC) trying to GET 0 bytes or into NULL buf");
    goto error_exit;
  }

  request_id = (( (uint64_t)proc)<<32) | INC_COUNTER(curr_cookie);

  if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
    log_err("Tried posting from a buffer that's not registered");
    goto error_exit;
  }

  rc = __photon_setup_request_direct(&rbuf, proc, REQUEST_FLAG_USERID, 1, local, request_id);
  if (rc != PHOTON_OK) {
    dbg_trace("Could not setup direct buffer request");
    goto error_exit;
  }
  
  rbuf.addr = (uintptr_t)rptr;
  rbuf.size = size;
  rbuf.priv = priv;
  
  rc = __photon_backend->rdma_get(proc, (uintptr_t)ptr, (uintptr_t)rptr, size, &(db->buf), &rbuf, request_id, 0);
  if (rc != PHOTON_OK) {
    dbg_err("RDMA GET (PWC data) failed for 0x%016lx", request_id);
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

  // only pop pending local completions when asking for them
  if (flags & PHOTON_PROBE_EVQ) {
    SLIST_LOCK(&pending_pwc_list);
    req = SLIST_FIRST(&pending_pwc_list);
    if (req) {
      *flag = 1;
      *request = req->id;
      SLIST_REMOVE_HEAD(&pending_pwc_list, slist);
      SLIST_UNLOCK(&pending_pwc_list);
      SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
      dbg_trace("returning local pwc request: 0x%016lx", req->id);
      return PHOTON_OK;
    }
    else
      SLIST_UNLOCK(&pending_pwc_list);
  }

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
    for (i=start; i<end; i++) {
      // check eager region first
      eb = photon_processes[i].local_pwc_buf;
      hdr = (photon_eb_hdr *)&(eb->data[eb->offset]);
      if (hdr->head == UINT8_MAX) {
        // now check for tail flag (or we could return to check later)
	uint16_t size = hdr->length;
	photon_rid req = hdr->request;
	uintptr_t addr = hdr->addr;
        volatile uint8_t *tail = (uint8_t*)((uintptr_t)hdr + sizeof(*hdr) + size);
	int tadd = ((uintptr_t)tail % PWC_ALIGN != 0)?(PWC_ALIGN - (uintptr_t)tail % PWC_ALIGN):0;
	tail += tadd;
        while (*tail != UINT8_MAX)
          ;
        memcpy((void*)addr, (void*)((uintptr_t)hdr + sizeof(*hdr)), size);
        *request = req;
        *flag = 1;
        NEXT_EAGER_BUF(eb, EB_MSG_SIZE(size+tadd));
        if ((_photon_ebsize - eb->offset) < EB_MSG_SIZE(__photon_config->cap.small_pwc_size))
          eb->offset = 0;
        dbg_trace("copied message of size %u into 0x%016lx for request 0x%016lx",
                 size, addr, req);
        memset((void*)hdr, 0, EB_MSG_SIZE(size+tadd));
        return PHOTON_OK;
      }

      // then check pwc ledger
      ledger = photon_processes[i].local_pwc_ledger;
      entry_iter = &(ledger->entries[ledger->curr]);
      if (entry_iter->request != (photon_rid) UINT64_MAX) {
        *request = entry_iter->request;
        *flag = 1;
        entry_iter->request = UINT64_MAX;
        NEXT_LEDGER_ENTRY(photon_processes[i].local_pwc_ledger);
        dbg_trace("popped ledger event with id: 0x%016lx", *request);
        return PHOTON_OK;
      }
    }
  }
  
  // we found something to process
  if (cookie != UINT64_MAX) {
    if (htable_lookup(pwc_reqtable, cookie, (void**)&req) == 0) {
      // set flag and request only if we have processed the number of outstanding
      // events expected for this reqeust
      if ( (--req->num_entries) == 0) {
	*flag = 1;
	*request = req->id;
	dbg_trace("completed and removing pwc request: 0x%016lx", cookie);
	htable_remove(pwc_reqtable, cookie, NULL);
	SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
	return PHOTON_OK;
      }
    }
    else if (htable_lookup(reqtable, cookie, (void**)&req) == 0) {
      dbg_trace("got CQ event for a non-PWC request: 0x%016lx", cookie);
      __photon_handle_cq_event(req, cookie);
    }
    else {
      dbg_trace("got CQ event not tracked: 0x%016lx", cookie);
      goto error_exit;
    }
  }
  
  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}
