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

static int photon_pwc_safe_ledger(photonRequest req);
static int photon_pwc_test_ledger(int proc, int *ret_offset);
static int photon_pwc_try_ledger(photonRequest req, int curr);
static int photon_pwc_safe_packed(photonRequest req);
static int photon_pwc_test_packed(int proc, uint64_t size, int *ret_offset);
static int photon_pwc_try_packed(photonRequest req, int offset);
static int photon_pwc_try_gwc(photonRequest req);
static int photon_pwc_handle_comp_req(photonRequest req, int *flag, photon_rid *r);

static two_lock_queue_t  *comp_q;
static pwc_recv_ledger  **pledgers;

int photon_pwc_init(photonConfig cfg) {
  comp_q = sync_two_lock_queue_new();
  if (!comp_q) {
    log_err("Could not allocate PWC completion queue");
    goto error_exit;
  }

  pledgers = (pwc_recv_ledger**)malloc(_photon_nproc*sizeof(pwc_recv_ledger*));
  if (!pledgers) {
    log_err("Could not allocate PWC recv ledgers");
    goto error_exit;
  }
  for (int i = 0; i < _photon_nproc; i++) {
    pledgers[i] = (pwc_recv_ledger*)calloc(_LEDGER_SIZE, sizeof(pwc_recv_ledger));
  }
  
  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

int photon_pwc_add_req(photonRequest req) {

  //photonRequestTable rt;
  //rt = photon_processes[req->proc].request_table;
  //sync_two_lock_queue_enqueue(rt->comp_q, req);

  sync_two_lock_queue_enqueue(comp_q, req);
  dbg_trace("Enqueing completed request: 0x%016lx", req->id);
  return PHOTON_OK;
}

photonRequest photon_pwc_pop_req(int proc) {
  return sync_two_lock_queue_dequeue(comp_q);

  /*
  photonRequest req;
  photonRequestTable rt;
  int i, start, end;

  if (proc == PHOTON_ANY_SOURCE) {
    start = 0;
    end = _photon_nproc;
  }
  else {
    start = proc;
    end = proc+1;
    assert(IS_VALID_PROC(start));
  }

  for (i=start; i<end; i++) {
    rt = photon_processes[i].request_table;
    req = sync_two_lock_queue_dequeue(rt->comp_q);
    if (req) {
      return req;
    }
  }

  return NULL;
  */
}

static int photon_pwc_check_gwc_align(photonBuffer lbuf, photonBuffer rbuf, uint64_t size) {
  int *align;
  int asize;
  int rc;

  rc =  __photon_backend->get_info(NULL, 0, (void**)&align, &asize, PHOTON_GET_ALIGN);
  if (rc != PHOTON_OK) {
    dbg_warn("Could not get alignment info from backend");
    *align = 0;
  }
  
  if (!TEST_ALIGN(lbuf->addr, *align) ||
      !TEST_ALIGN(rbuf->addr, *align) ||
      !TEST_ALIGN(size, *align)) {
    return PHOTON_ERROR;
  }
  
  return PHOTON_OK;
}

static int photon_pwc_gwc_put(photonRequest req, uint64_t ssize, int offset) {
  photonEagerBuf eb;
  photon_eb_hdr *hdr;
  uint64_t asize, imm_data = 0;
  uintptr_t eager_addr;
  uint8_t *tail;
  int rc, moffset;

  dbg_trace("Performing GWC-PUT for req: 0x%016lx", req->id);

  asize = ALIGN(EB_MSG_SIZE(ssize), PWC_ALIGN);
  eb = photon_processes[req->proc].remote_pwc_buf;
  
  // make sure the original request size is encoded in the local/remote bufs
  req->local_info.buf.size = req->size;
  req->remote_info.buf.size = req->size;

  // override the request size for this packed GWC-PUT
  req->size          = ssize;
  req->flags        |= REQUEST_FLAG_1PWC;
  req->rattr.events  = 1; // N/A, waits for PWC reply from remote
  req->rattr.cookie  = ( (uint64_t)REQUEST_COOK_GPWC<<32) | req->proc;
    
  eager_addr = (uintptr_t)eb->remote.addr + offset;
  hdr = (photon_eb_hdr *)&(eb->data[offset]);
  hdr->header  = UINT8_MAX;
  hdr->request = PWC_COMMAND_PWC_REQ;
  hdr->addr    = req->id;
  hdr->length  = ssize;
  hdr->footer  = UINT8_MAX;
  
  moffset = sizeof(*hdr);
  // copy the local buffer
  memcpy((void*)((uintptr_t)hdr + moffset), (void*)&req->local_info.buf,
	 sizeof(req->local_info.buf));
  moffset += sizeof(req->local_info.buf);
  // copy the remote buffer
  memcpy((void*)((uintptr_t)hdr + moffset), (void*)&req->remote_info.buf,
	 sizeof(req->remote_info.buf));
  // set a tail flag, the last byte in aligned buffer
  tail = (uint8_t*)((uintptr_t)hdr + asize - 1);
  *tail = UINT8_MAX;
  
  imm_data = ENCODE_RCQ_32(PHOTON_ETYPE_ONE, PHOTON_EFLAG_PACK, _photon_myrank);
  
  rc = __photon_backend->rdma_put(req->proc, (uintptr_t)hdr, (uintptr_t)eager_addr, asize,
				  &(shared_storage->buf), &eb->remote, req->rattr.cookie,
				  imm_data, RDMA_FLAG_WITH_IMM);
  if (rc != PHOTON_OK) {
    dbg_err("RDMA PUT (GWC-PUT) failed for 0x%016lx", req->rattr.cookie);
    goto error_exit;
  }  
  
  dbg_trace("Posted GWC-PUT Request: 0x%016lx", req->id);
  
  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}

static int photon_pwc_process_command(int proc, photon_rid cmd, uintptr_t id,
				      uint16_t size, void *ptr) {

  photon_rid pwc_cmd = cmd>>56<<56;
  switch (pwc_cmd) {
  case PWC_COMMAND_PWC_REQ:
    {
      int rc;
      photon_rid rid;
      struct photon_buffer_t lbuf, rbuf;
      // switch the sent lbuf/rbuf
      memcpy(&rbuf, ptr, sizeof(rbuf));
      memcpy(&lbuf, ptr+sizeof(rbuf), sizeof(lbuf));
      rid = PWC_COMMAND_PWC_REP | id;
      rc = _photon_put_with_completion(proc, lbuf.size, &lbuf, &rbuf, rid, rid,
				       PHOTON_REQ_PWC_NO_LCE);
      if (rc != PHOTON_OK) {
	log_err("Could not complete PWC_REQ command");
	goto error_exit;
      }
    }
    break;
  case PWC_COMMAND_PWC_REP:
    {
      // command encodes the GWC request that initiated the PWC
      photonRequest req;
      photon_rid rid = cmd & ~(PWC_COMMAND_MASK);

      // copy the payload if reply was packed
      if (id && size) {
	memcpy((void*)id, ptr, size);
      }

      req = photon_lookup_request(rid);
      if (!req) {
	log_err("Could not find request in PWC_REP");
	goto error_exit;
      }
      photon_pwc_add_req(req);
    }
    break;
  default:
    log_err("Uknown PWC command received: 0x%016lx", cmd);
    break;
  }

  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

static int photon_pwc_handle_comp_req(photonRequest req, int *flag, photon_rid *request) {
  int rc;

  // sometimes the requestor doesn't care about the completion
  if (! (req->flags & REQUEST_FLAG_NO_LCE)) {
    *flag = 1;
    *request = req->local_info.id;
  }
  
  if (req->flags & REQUEST_FLAG_ROP) {
    // sends a remote completion after the GWC
    // this GWC request now becomes a PWC
    // and we reap the put completion internally
    req->flags |= (REQUEST_FLAG_NO_LCE | REQUEST_FLAG_CMD);
    req->flags &= ~(REQUEST_FLAG_ROP);
    req->rattr.cookie = req->id;
    rc = photon_pwc_safe_ledger(req);
    if (rc != PHOTON_OK) {
      photonRequestTable rt;
      rt = photon_processes[req->proc].request_table;
      sync_two_lock_queue_enqueue(rt->pwc_q, req);
      sync_fadd(&rt->pcount, 1, SYNC_RELAXED);
      dbg_trace("Enqueing ROP PWC req: 0x%016lx", req->id);
    }
    goto no_free;
  }

  dbg_trace("Completed and removing PWC/GWC request: 0x%016lx (lid=0x%016lx)",
	    req->id, req->local_info.id);
  photon_free_request(req);
  
 no_free:
  return PHOTON_OK;
}

static int photon_pwc_process_queued_gwc(int proc, photonRequestTable rt) {
  photonRequest req;
  uint32_t val;
  int rc;

  // make sure there are TX resources first
  rc = __photon_backend->tx_size_left(proc);
  if (rc < 1) {
    goto error_resource;
  }
     
  do {
    val = sync_load(&rt->gcount, SYNC_RELAXED);
  } while (val && !sync_cas(&rt->gcount, val, val-1, SYNC_RELAXED, SYNC_RELAXED));
  
  if (!val) {
    return PHOTON_ERROR;
  }
  
  req = sync_two_lock_queue_dequeue(rt->gwc_q);
  assert(req);

  rc = photon_pwc_try_gwc(req);
  if (rc != PHOTON_OK) {
    if (rc == PHOTON_ERROR_RESOURCE) {
      goto error_resource;
    }
    dbg_err("RDMA GET (PWC data) failed for 0x%016lx", req->rattr.cookie);
    goto error_exit;
  }
  
  dbg_trace("Posted Request: %d/0x%016lx/0x%016lx", proc, req->local_info.id, req->rattr.cookie);
  
  return PHOTON_OK;

 error_resource:
  return PHOTON_ERROR_RESOURCE;

 error_exit:
  return PHOTON_ERROR;
}

static int photon_pwc_process_queued_pwc(int proc, photonRequestTable rt) {
  photonRequest req;
  uint32_t val;
  int offset, rc;

  do {
    val = sync_load(&rt->pcount, SYNC_RELAXED);
  } while (val && !sync_cas(&rt->pcount, val, val-1, SYNC_RELAXED, SYNC_RELAXED));

  if (!val) {
    return PHOTON_ERROR;
  }
  
  sync_tatas_acquire(&rt->ledg_loc);
  {
    // only dequeue a request if there is one and we can send it
    rc = photon_pwc_test_ledger(proc, &offset);
    if (rc == PHOTON_OK) {
      req = sync_two_lock_queue_dequeue(rt->pwc_q);
      assert(req);
      rc = photon_pwc_try_ledger(req, offset);
      if (rc != PHOTON_OK) {
	dbg_err("Could not send queued PWC request");
      }
    }
    else {
      // if we could not send, indicate that the request is still in the queue
      sync_fadd(&rt->pcount, 1, SYNC_RELAXED);
    }
  }
  sync_tatas_release(&rt->ledg_loc);
  
  return PHOTON_OK;
}

static int photon_pwc_try_gwc(photonRequest req) {
  int rc;
  photonBI db;
  photonRequestTable rt;
  
  rt = photon_processes[req->proc].request_table;

  if (!req->local_info.buf.priv.key0 && !req->local_info.buf.priv.key1) {
    if (buffertable_find_containing( (void *)req->local_info.buf.addr,
				     req->size, &db) != 0) {
      log_err("Tried posting from a buffer that's not registered");
      goto error_exit;
    }

    req->local_info.buf.priv = db->buf.priv;
  }
  
  rc = photon_pwc_check_gwc_align(&req->local_info.buf,
				  &req->remote_info.buf,
				  req->size);
  if (rc != PHOTON_OK) {
    // turn this get into a put from the target
    // the payload is the local and remote GWC buffers
    uint64_t ssize = sizeof(req->local_info.buf) + sizeof(req->remote_info.buf);
    int offset;
    sync_tatas_acquire(&rt->pack_loc);
    {
      rc = photon_pwc_test_packed(req->proc, ssize, &offset);
      if (rc == PHOTON_OK) {
	rc = photon_pwc_gwc_put(req, ssize, offset);
      }
    }
    sync_tatas_release(&rt->pack_loc);
    
    if (rc != PHOTON_OK) {
      goto error_resource;
    }
  }
  else {
    rc = __photon_backend->rdma_get(req->proc, req->local_info.buf.addr,
				    req->remote_info.buf.addr, req->size,
				    &req->local_info.buf,
				    &req->remote_info.buf,
				    req->rattr.cookie, RDMA_FLAG_NIL);
    
    if (rc != PHOTON_OK) {
      dbg_err("RDMA GET (PWC data) failed for 0x%016lx", req->rattr.cookie);
      goto error_exit;
    }
  }
  
  dbg_trace("Posted GWC Request: %d/0x%016lx/0x%016lx", req->proc,
	    req->local_info.id,
	    req->remote_info.id);  
  
  return PHOTON_OK;

 error_resource:
  return PHOTON_ERROR_RESOURCE;
  
 error_exit:
  return PHOTON_ERROR;
}

static int photon_pwc_safe_packed(photonRequest req) {
  photonRequestTable rt;
  int rc, proc, offset;

  proc = req->proc;
  rt = photon_processes[proc].request_table;
  
  sync_tatas_acquire(&rt->pack_loc);
  {
    rc = photon_pwc_test_packed(req->proc, req->size, &offset);
    if (rc == PHOTON_OK) {
      rc = photon_pwc_try_packed(req, offset);
    }
  }
  sync_tatas_release(&rt->pack_loc);
  
  return rc;
}

static int photon_pwc_test_packed(int proc, uint64_t size, int *ret_offset) {
  photonEagerBuf eb;
  uint64_t asize;
  int offset;
  
  // only do packed if size makes sense
  if ((size <= 0) ||
      (size > _photon_spsize) ||
      (size > _photon_ebsize)) {
    return PHOTON_ERROR_RESOURCE;
  }

  // keep buffer offsets aligned
  asize = ALIGN(EB_MSG_SIZE(size), PWC_ALIGN);

  eb = photon_processes[proc].remote_pwc_buf;
  offset = photon_rdma_eager_buf_get_offset(proc, eb, asize,
		   ALIGN(EB_MSG_SIZE(_photon_spsize), PWC_ALIGN));
  if (offset < 0) {
    return PHOTON_ERROR_RESOURCE;
  }
  *ret_offset = offset;
  return PHOTON_OK;
}

static int photon_pwc_try_packed(photonRequest req, int offset) {
  // see if we should pack into an eager buffer and send in one put
  photonEagerBuf eb;
  photon_eb_hdr *hdr;
  uint64_t asize, imm_data = 0;
  uintptr_t eager_addr;
  uint8_t *tail;
  int rc;
  
  req->flags        |= REQUEST_FLAG_1PWC;
  req->rattr.events  = 1;
  
  asize = ALIGN(EB_MSG_SIZE(req->size), PWC_ALIGN);    
  eb = photon_processes[req->proc].remote_pwc_buf;
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
  
  imm_data = ENCODE_RCQ_32(PHOTON_ETYPE_ONE, PHOTON_EFLAG_PACK, _photon_myrank);
  
  rc = __photon_backend->rdma_put(req->proc, (uintptr_t)hdr, (uintptr_t)eager_addr, asize,
				  &(shared_storage->buf), &eb->remote, req->rattr.cookie,
				  imm_data, RDMA_FLAG_WITH_IMM);
  if (rc != PHOTON_OK) {
    dbg_err("RDMA PUT (PWC EAGER) failed for 0x%016lx", req->rattr.cookie);
    goto error_exit;
  }
  
  dbg_trace("Posted PWC Request: %d/0x%016lx/0x%016lx", req->proc,
	    req->local_info.id,
	    req->remote_info.id);
  
  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}

static int photon_pwc_safe_ledger(photonRequest req) {
  photonRequestTable rt;
  int rc, proc, offset;

  proc = req->proc;
  rt = photon_processes[proc].request_table;

  sync_tatas_acquire(&rt->ledg_loc);
  {
    rc = photon_pwc_test_ledger(req->proc, &offset);
    if (rc == PHOTON_OK) {
      rc = photon_pwc_try_ledger(req, offset);
    }
  }
  sync_tatas_release(&rt->ledg_loc);
  
  return rc;
}

static int photon_pwc_test_ledger(int proc, int *ret_offset) {
  photonLedger l;
  int curr;
  l = photon_processes[proc].remote_pwc_ledger;
  curr = photon_rdma_ledger_get_next(proc, l);
  if (curr < 0) {
    return PHOTON_ERROR_RESOURCE;
  }
  *ret_offset = curr;
  return PHOTON_OK;
}

static int photon_pwc_try_ledger(photonRequest req, int curr) {
  photonBI db;
  photonLedgerEntry entry;
  uintptr_t rmt_addr;
  uint64_t imm_data = 0;
  int rc, iflags;
  
  req->flags |= REQUEST_FLAG_2PWC;
  req->rattr.events = 1;
  iflags = PHOTON_EFLAG_LEDG;

  if ((req->size) > 0 && !(req->flags & REQUEST_FLAG_CMD)) {
    if (!req->local_info.buf.priv.key0 && !req->local_info.buf.priv.key1) {
      if (buffertable_find_containing( (void *)req->local_info.buf.addr,
				       req->size, &db) != 0) {
	log_err("Tried posting from a buffer that's not registered");
	goto error_exit;
      }
      req->local_info.buf.priv = db->buf.priv;
    }
    
    if (! (req->flags & REQUEST_FLAG_NO_RCE))
      req->rattr.events = 2;
    
    imm_data = ENCODE_RCQ_32(PHOTON_ETYPE_TWO, curr, _photon_myrank);

    rc = __photon_backend->rdma_put(req->proc,
				    req->local_info.buf.addr,
				    req->remote_info.buf.addr,
				    req->size,
				    &req->local_info.buf,
				    &req->remote_info.buf,
				    req->rattr.cookie,
				    imm_data,
				    RDMA_FLAG_WITH_IMM);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC data) failed for 0x%016lx", req->rattr.cookie);
      goto error_exit;
    }
    iflags |= PHOTON_EFLAG_2ND;
  }
  else {
    iflags |= PHOTON_EFLAG_1ST;
  }
  
  if (! (req->flags & REQUEST_FLAG_NO_RCE)) {
    photonLedger l = photon_processes[req->proc].remote_pwc_ledger;
    assert(curr >= 0);
    entry = &(l->entries[curr]);
    entry->request = req->remote_info.id;

    rmt_addr = (uintptr_t)l->remote.addr + (sizeof(*entry) * curr);
    dbg_trace("putting into remote ledger addr: 0x%016lx", rmt_addr);
    
    imm_data = ENCODE_RCQ_32(PHOTON_ETYPE_ONE, iflags, _photon_myrank);

    rc = __photon_backend->rdma_put(req->proc, (uintptr_t)entry, rmt_addr,
				    sizeof(*entry), &(shared_storage->buf),
				    &(l->remote), req->rattr.cookie,
				    imm_data, RDMA_FLAG_WITH_IMM);

    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT (PWC comp) failed for 0x%016lx", req->rattr.cookie);
      goto error_exit;
    }
  }
  
  dbg_trace("Posted PWC Request: %d/0x%016lx/0x%016lx/0x%016lx", req->proc,
	    req->rattr.cookie,
	    req->local_info.id,
	    req->remote_info.id);
  
  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

int _photon_put_with_completion(int proc, uint64_t size,
				photonBuffer lbuf,
				photonBuffer rbuf,
				photon_rid local, photon_rid remote,
				int flags) {
  photonRequest req;
  photonRequestTable rt;
  int rc;

  dbg_trace("(%d, size: %lu, lid: 0x%016lx, rid: 0x%016lx, flags: %d)", proc,
	    size, local, remote, flags);  

  if (size && !lbuf) {
    log_err("Trying to put size %lu and NULL lbuf", size);
    goto error_exit;
  }

  if (size && !rbuf) {
    log_err("Trying to put size %lu and NULL rbuf", size);
    goto error_exit;
  }

  if (!size && (flags & PHOTON_REQ_PWC_NO_RCE)) {
    dbg_warn("Nothing to send and no remote completion requested!");
    return PHOTON_OK;
  }

  if (size && !rbuf->priv.key0 && !rbuf->priv.key1) {
    dbg_warn("No remote buffer keys specified!");
  }
  
  req = photon_setup_request_direct(lbuf, rbuf, size, proc, 0);
  if (!req) {
    dbg_err("Could not allocate request");
    goto error_exit;
  }
  
  req->op             = REQUEST_OP_PWC;
  req->local_info.id  = local;
  req->remote_info.id = remote;
  
  // control the return of the local id
  if (flags & PHOTON_REQ_PWC_NO_LCE) {
    req->flags |= REQUEST_FLAG_NO_LCE;
  }

  // control the return of the remote id
  if (flags & PHOTON_REQ_PWC_NO_RCE) {
    req->flags |= REQUEST_FLAG_NO_RCE;
    return photon_pwc_try_ledger(req, 0);
  }

  rt = photon_processes[proc].request_table;

  // process any queued requests for this peer first
  rc = photon_pwc_process_queued_pwc(proc, rt);
  if (rc == PHOTON_OK) {
    goto queue_exit;
  }

  // otherwise try to send the current request
  rc = photon_pwc_safe_packed(req);
  if (rc == PHOTON_ERROR_RESOURCE) {
    rc = photon_pwc_safe_ledger(req);
  }
  
  if ((rc == PHOTON_ERROR) || (rc == PHOTON_OK)) {
    return rc;
  }

 queue_exit:
  sync_two_lock_queue_enqueue(rt->pwc_q, req);
  sync_fadd(&rt->pcount, 1, SYNC_RELAXED);
  dbg_trace("Enqueued PWC request: 0x%016lx", req->id);
  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

int _photon_get_with_completion(int proc, uint64_t size,
				photonBuffer lbuf,
				photonBuffer rbuf,
				photon_rid local, photon_rid remote,
				int flags) {
  photonRequest req;
  photonRequestTable rt;
  int rc;
  
  dbg_trace("(%d, size: %lu, lid: 0x%016lx, rid: 0x%016lx, flags: %d)", proc,
	    size, local, remote, flags);

  if (size && !rbuf) {
    log_err("Tring to get size %lu and NULL rbuf", size);
    goto error_exit;
  }

  if (!size || !lbuf) {
    log_err("Trying to get 0 bytes or into NULL lbuf");
    goto error_exit;
  }

  if (size && !rbuf->priv.key0 && !rbuf->priv.key1) {
    dbg_warn("No remote buffer keys specified!");
  }

  req = photon_setup_request_direct(lbuf, rbuf, size, proc, 1);
  if (req == NULL) {
    dbg_trace("Could not setup direct buffer request");
    goto error_exit;
  }
  
  req->op = REQUEST_OP_PWC;
  req->local_info.id = local;
  req->remote_info.id = remote;

  // control the return of the local id
  if (flags & PHOTON_REQ_PWC_NO_LCE) {
    req->flags |= REQUEST_FLAG_NO_LCE;
  }

  // control the sending of the remote id to proc
  if (! (flags & PHOTON_REQ_PWC_NO_RCE)) {
    req->flags |= REQUEST_FLAG_ROP;
  }

  // make sure there are TX resources first
  rc = __photon_backend->tx_size_left(proc);
  if (rc < 1) {
    goto queue_exit;
  }

  rt = photon_processes[proc].request_table;

  // process any queued requests for this peer first
  rc = photon_pwc_process_queued_gwc(proc, rt);
  if (rc == PHOTON_OK) {
    goto queue_exit;
  }
  
  rc = photon_pwc_try_gwc(req);
  if (rc != PHOTON_OK) {
    if (rc == PHOTON_ERROR_RESOURCE) {
      goto queue_exit;
    }
    dbg_err("RDMA GET (PWC data) failed for 0x%016lx", req->rattr.cookie);
    goto error_exit;
  }
  
  return PHOTON_OK;

 queue_exit:
  sync_two_lock_queue_enqueue(rt->gwc_q, req);
  sync_fadd(&rt->gcount, 1, SYNC_RELAXED);
  dbg_trace("Enqueued GWC request: 0x%016lx", req->id);
  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}

static int photon_pwc_probe_local(int proc, int *flag, photon_rid *request) {
  photonRequest req;
  photon_rid cookie = NULL_REQUEST;
  int rc;

  // handle any pwc requests that were popped in some other path  
  req = photon_pwc_pop_req(proc);
  if (req != NULL) {
    photon_pwc_handle_comp_req(req, flag, request);
    goto exit;
  }
  
  rc = __photon_get_event(proc, &cookie);
  if (rc == PHOTON_EVENT_ERROR) {
    dbg_err("Error getting event, rc=%d", rc);
    goto error_exit;
  }
  else if (rc == PHOTON_EVENT_OK) {
    // we found an event to process
    rc = __photon_handle_cq_event(NULL, cookie, &req);
    if (rc == PHOTON_EVENT_ERROR) {
      goto error_exit;
    }
    else if ((rc == PHOTON_EVENT_REQCOMP) && req &&
	     (req->op == REQUEST_OP_PWC)) {
      photon_pwc_handle_comp_req(req, flag, request);
      goto exit;
    }
    else {
      dbg_trace("PWC probe handled non-completion event: 0x%016lx", cookie);
    }
  }
  
  return PHOTON_EVENT_NONE;

 exit:
  return PHOTON_EVENT_REQFOUND;

 error_exit:
  return PHOTON_EVENT_ERROR;
}

static int photon_pwc_handle_ooo(uint64_t imm, int *flag, photon_rid *request, int *src) {
  int type = DECODE_RCQ_32_TYPE(imm);
  int proc = DECODE_RCQ_32_PROC(imm);

  pwc_recv_ledger *l = pledgers[proc];  

  switch (type) {
  case PHOTON_ETYPE_TWO:
    {
      int loc = DECODE_RCQ_32_FLAG(imm);
      int count = sync_addf(&l[loc].cnt, 1, SYNC_RELAXED);
      if (count == 2) {
	*flag = 1;
	*src = proc;
	*request = l[loc].rid;
	l[loc].cnt = 0;
	goto exit_ok;
      }
      break;
    }
  case PHOTON_ETYPE_ONE:
    {
      // src contains the offset here
      // request is already set in call
      int loc = *src;
      int count = sync_addf(&l[loc].cnt, 1, SYNC_RELAXED);
      if (count == 2) {
	*flag = 1;
	l[loc].cnt = 0;
	goto exit_ok;
      }
      else {
	l[loc].rid = *request;
      }
    }
    break;
  default:
    break;
  }
  
  return PHOTON_EVENT_NONE;

 exit_ok:
  return PHOTON_OK;
}

static int photon_pwc_probe_ledger(int proc, int *flag, photon_rid *request, int *src) {
  photonLedger ledger;
  photonLedgerEntry entry_iter;
  photonEagerBuf eb;
  photon_eb_hdr *hdr;
  photon_rid cookie = NULL_REQUEST;
  uint64_t imm;
  int i, rc, start, end, rflags, etype;
  int scan_packed = 1, scan_ledger = 1;
  int rcq = 0;
  
  if (proc == PHOTON_ANY_SOURCE) {
    rc = __photon_get_revent(proc, &cookie, &imm);
    if (rc == PHOTON_EVENT_OK) {

      etype = DECODE_RCQ_32_TYPE(imm);
      // Check for completion of first part of a 2PWC request
      if (etype == PHOTON_ETYPE_TWO)
	return photon_pwc_handle_ooo(imm, flag, request, src);
      
      start = DECODE_RCQ_32_PROC(imm);
      end = start+1;
      assert(IS_VALID_PROC(start));

      rflags = DECODE_RCQ_32_FLAG(imm);
      scan_packed = (rflags & PHOTON_EFLAG_PACK) ? 1 : 0;
      scan_ledger = (rflags & PHOTON_EFLAG_LEDG) ? 1 : 0;
      assert(scan_packed || scan_ledger);
      
      rcq = 1;
    }
    // If we don't have remote completion support, must scan entire ledger
    else if (rc == PHOTON_EVENT_NOTIMPL) {
      start = 0;
      end = _photon_nproc;
    }
    // if no event or error, simply return with code
    else {
      return rc;
    }
  }
  else {
    // simply clear revents as necessary
    __photon_get_revent(PHOTON_ANY_SOURCE, &cookie, &imm);
    // and probe ledger directly
    start = proc;
    end = proc+1;
    assert(IS_VALID_PROC(start));
  }
  
  uint64_t offset, curr, new, left;
  for (i=start; i<end; i++) {
    // first we check the packed buffer space if necessary
    if (scan_packed) {
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
	void *payload = (void*)(uintptr_t)hdr+sizeof(*hdr);
	if (sync_cas(&eb->curr, curr, new+asize, SYNC_RELAXED, SYNC_RELAXED)) {
	  // now check for tail flag (or we could return to check later)
	  volatile uint8_t *tail = (uint8_t*)((uintptr_t)hdr + asize - 1);
	  while (*tail != UINT8_MAX)
	    ;
	  // check for PWC commands to process
	  if (req & PWC_COMMAND_MASK) {
	    photon_pwc_process_command(i, req, addr, size, payload);
	  }
	  else {
	    assert(addr);
	    assert(payload);
	    memcpy((void*)addr, payload, size);
	    *request = req;
	    *src = i;
	    *flag = 1;
	    dbg_trace("Copied message of size %u into 0x%016lx for request 0x%016lx",
		      size, addr, req);
	  }
	  memset((void*)hdr, 0, asize);
	  sync_store(&eb->prog, new+asize, SYNC_RELAXED);
	  goto exit;
	}
      }
    }
    
    if (scan_ledger) {
      // then check pwc ledger slots
      ledger = photon_processes[i].local_pwc_ledger;
      curr = sync_load(&ledger->curr, SYNC_RELAXED);
      offset = curr & (ledger->num_entries - 1);
      entry_iter = &(ledger->entries[offset]);
      if (entry_iter->request != (photon_rid) UINT64_MAX &&
	  sync_cas(&ledger->curr, curr, curr+1, SYNC_RELAXED, SYNC_RELAXED)) {

	*request = entry_iter->request;
	*src = i;	

	if (rcq && (rflags & PHOTON_EFLAG_2ND)) {
	  // we might have to wait for 1st PUT to complete
	  photon_pwc_handle_ooo(imm, flag, request, (int*)&offset);
	}
	else {
	  *flag = 1;
	}
	
	// check for PWC commands to process
	if (entry_iter->request & PWC_COMMAND_MASK) {
	  photon_pwc_process_command(i, entry_iter->request, 0, 0, NULL);
	  // don't indicate completion to caller if internal command
	  *flag = 0;
	}
	
	entry_iter->request = UINT64_MAX;
	sync_fadd(&ledger->prog, 1, SYNC_RELAXED);
	dbg_trace("Popped ledger event with id: 0x%016lx (%lu)", *request, *request);
	goto exit;
      }
    }
  }
    
  if (rcq)
    log_err("Missing RCQ lookup: %d, %d", scan_packed, scan_ledger);

  return PHOTON_EVENT_NONE;
  
 exit:
  return PHOTON_EVENT_REQFOUND;
}  

int _photon_probe_completion(int proc, int *flag, int *remaining,
			     photon_rid *request, int *src, int flags) {
  int i, rc;

  *flag = 0;
  *src = proc;

  // check local CQs
  if (flags & PHOTON_PROBE_EVQ) {
    rc = photon_pwc_probe_local(proc, flag, request);
    if (rc == PHOTON_EVENT_REQFOUND) {
      goto exit;
    }
    else if (rc == PHOTON_EVENT_ERROR) {
      goto error_exit;
    }
  }
  
  // check recv ledger
  if (flags & PHOTON_PROBE_LEDGER) {
    rc = photon_pwc_probe_ledger(proc, flag, request, src);
    if (rc == PHOTON_EVENT_REQFOUND) {
      goto exit;
    }
    else if (rc == PHOTON_EVENT_ERROR) {
      goto error_exit;
    }
  }
    
  // process any queued requests, only when EVQ requested
  if ((rc == PHOTON_EVENT_NONE) && (flags & PHOTON_PROBE_EVQ)) {
    for (i=0; i<_photon_nproc; i++) {
      photon_pwc_process_queued_gwc(i, photon_processes[i].request_table);
      photon_pwc_process_queued_pwc(i, photon_processes[i].request_table);
    }
  }

 exit:
  if (remaining) {
    *remaining = photon_count_request(proc);
    dbg_trace("%d requests remaining", *remaining);
  }

  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}
