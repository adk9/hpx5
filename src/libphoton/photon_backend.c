#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "libphoton.h"
#include "photon_backend.h"
#include "photon_buffertable.h"
#include "photon_exchange.h"
#include "htable.h"
#include "counter.h"
#include "logging.h"
#include "squeue.h"

photonBI shared_storage;

static photonMsgBuf sendbuf;
static photonMsgBuf recvbuf;
static ProcessInfo *photon_processes;
static htable_t *reqtable, *ledger_reqtable, *sr_reqtable;
static photonRequest requests;
static int num_requests;
static LIST_HEAD(freereqs, photon_req_t) free_reqs_list;
static SLIST_HEAD(pendingrecvlist, photon_req_t) pending_recv_list;
static SLIST_HEAD(pendingmemregs, photon_mem_register_req) pending_mem_register_list;

DEFINE_COUNTER(curr_cookie, uint32_t)

/* default backend methods */
static int _photon_initialized(void);
static int _photon_init(photonConfig cfg, ProcessInfo *info, photonBI ss);
static int _photon_finalize(void);
static int _photon_register_buffer(void *buffer, uint64_t size);
static int _photon_unregister_buffer(void *buffer, uint64_t size);
static int _photon_test(uint32_t request, int *flag, int *type, photonStatus status);
static int _photon_wait(uint32_t request);
static int _photon_send(photonAddr addr, void *ptr, uint64_t size, int flags, uint64_t *request);
static int _photon_recv(uint64_t request, void *ptr, uint64_t size, int flags);
static int _photon_post_recv_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, uint32_t *request);
static int _photon_post_send_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, uint32_t *request);
static int _photon_post_send_request_rdma(int proc, uint64_t size, int tag, uint32_t *request);
static int _photon_wait_recv_buffer_rdma(int proc, int tag, uint32_t *request);
static int _photon_wait_send_buffer_rdma(int proc, int tag, uint32_t *request);
static int _photon_wait_send_request_rdma(int tag);
static int _photon_post_os_put(uint32_t request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset);
static int _photon_post_os_get(uint32_t request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset);
static int _photon_post_os_put_direct(int proc, void *ptr, uint64_t size, int tag, photonBuffer rbuf, uint32_t *request);
static int _photon_post_os_get_direct(int proc, void *ptr, uint64_t size, int tag, photonBuffer rbuf, uint32_t *request);
static int _photon_send_FIN(uint32_t request, int proc);
static int _photon_wait_any(int *ret_proc, uint32_t *ret_req);
static int _photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req);
static int _photon_probe_ledger(int proc, int *flag, int type, photonStatus status);
static int _photon_probe(photonAddr addr, int *flag, photonStatus status);
static int _photon_io_init(char *file, int amode, MPI_Datatype view, int niter);
static int _photon_io_finalize();

static int __photon_nbpop_event(photonRequest req);
static int __photon_nbpop_sr(photonRequest req);
static int __photon_nbpop_ledger(photonRequest req);
static int __photon_wait_ledger(photonRequest req);
static int __photon_wait_event(photonRequest req);

static int __photon_setup_request_direct(photonBuffer rbuf, int proc, int tag, uint32_t request);
static int __photon_setup_request_ledger(photonRILedgerEntry ri_entry, int proc, uint32_t *request);
static int __photon_setup_request_send(photonAddr addr, int *bufs, int nbufs, uint32_t request);
static photonRequest __photon_setup_request_recv(photonAddr addr, int msn, int msize, int bindex, int nbufs, uint64_t request);

static int __photon_handle_send_event(photonRequest req, uint64_t id);
static int __photon_handle_recv_event(uint64_t id);

/*
   We only want to spawn a dedicated thread for ledgers on
   multithreaded instantiations of the library (e.g. in xspd).
*/
#ifdef PHOTON_MULTITHREADED
static pthread_t ledger_watcher;
static void *__photon_req_watcher(void *arg);
#endif

struct photon_backend_t photon_default_backend = {
  .initialized = _photon_initialized,
  .init = _photon_init,
  .finalize = _photon_finalize,
  .get_dev_addr = NULL,
  .register_addr = NULL,
  .unregister_addr = NULL,
  .register_buffer = _photon_register_buffer,
  .unregister_buffer = _photon_unregister_buffer,
  .test = _photon_test,
  .wait = _photon_wait,
  .wait_ledger = _photon_wait,
  .send = _photon_send,
  .recv = _photon_recv,
  .post_recv_buffer_rdma = _photon_post_recv_buffer_rdma,
  .post_send_buffer_rdma = _photon_post_send_buffer_rdma,
  .post_send_request_rdma = _photon_post_send_request_rdma,
  .wait_recv_buffer_rdma = _photon_wait_recv_buffer_rdma,
  .wait_send_buffer_rdma = _photon_wait_send_buffer_rdma,
  .wait_send_request_rdma = _photon_wait_send_request_rdma,
  .post_os_put = _photon_post_os_put,
  .post_os_get = _photon_post_os_get,
  .post_os_put_direct = _photon_post_os_put_direct,
  .post_os_get_direct = _photon_post_os_get_direct,
  .send_FIN = _photon_send_FIN,
  .wait_any = _photon_wait_any,
  .wait_any_ledger = _photon_wait_any_ledger,
  .probe_ledger = _photon_probe_ledger,
  .probe = _photon_probe,
  .io_init = _photon_io_init,
  .io_finalize = _photon_io_finalize,
  .rdma_get = NULL,
  .rdma_put = NULL,
  .rdma_send = NULL,
  .rdma_recv = NULL,
  .get_event = NULL
};

static inline photonRequest __photon_get_request() {
  photonRequest req;

  LIST_LOCK(&free_reqs_list);
  req = LIST_FIRST(&free_reqs_list);
  if (req)
    LIST_REMOVE(req, list);
  LIST_UNLOCK(&free_reqs_list);
  
  if (!req) {
    dbg_info("Request list is empty, this should rarely happen");
    req = malloc(sizeof(struct photon_req_t));
  }

  req->flags = REQUEST_FLAG_NIL;

  return req;
}

/* request id has already been set  */
static int __photon_setup_request_direct(photonBuffer rbuf, int proc, int tag, uint32_t request) {
  photonRequest req;
  
  req = __photon_get_request();
  if (!req) {
    log_err("Couldn't allocate request\n");
    goto error_exit;
  }

  req->id = request;
  req->state = REQUEST_PENDING;
  req->type = EVQUEUE;
  req->proc = proc;
  req->tag = tag;
  
  /* fill in the internal buffer with the rbuf contents */
  memcpy(&req->remote_buffer, rbuf, sizeof(*rbuf));
  /* there is no matching request from the remote side, so fill in local values */
  req->remote_buffer.request = request;
  req->remote_buffer.tag = tag;
  
  dbg_info("Remote request: %u", request);
  dbg_info("Addr: %p", (void *)rbuf->addr);
  dbg_info("Size: %lu", rbuf->size);
  dbg_info("Tag: %d",	tag);
  dbg_info("Keys: 0x%016lx / 0x%016lx", rbuf->priv.key0, rbuf->priv.key1);
  
  dbg_info("Inserting the OS put request into the request table: %d/%d/%p", proc, request, req);
  if (htable_insert(reqtable, (uint64_t)request, req) != 0) {
    // this is bad, we've submitted the request, but we can't track it
    log_err("Couldn't save request in hashtable");
  }

  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

/* generates a new request for the received ledger event */
static int __photon_setup_request_ledger(photonRILedgerEntry ri_entry, int proc, uint32_t *request) {
  photonRequest req;
  uint32_t request_id;

  request_id = INC_COUNTER(curr_cookie);
  dbg_info("Incrementing curr_cookie_count to: %d", request_id);

  *request = request_id;
  
  req = __photon_get_request();
  if (!req) {
    log_err("Couldn't allocate request\n");
    goto error_exit;
  }
  req->id = request_id;
  req->state = REQUEST_NEW;
  req->proc = proc;
  
  /* save the remote buffer in the request */
  req->remote_buffer.request   = ri_entry->request;
  req->remote_buffer.tag       = ri_entry->tag;
  req->remote_buffer.buf.addr  = ri_entry->addr;
  req->remote_buffer.buf.size  = ri_entry->size;
  req->remote_buffer.buf.priv  = ri_entry->priv;
  
  dbg_info("Remote request: %u", ri_entry->request);
  dbg_info("Addr: %p", (void *)ri_entry->addr);
  dbg_info("Size: %lu", ri_entry->size);
  dbg_info("Tag: %d",	ri_entry->tag);
  dbg_info("Keys: 0x%016lx / 0x%016lx", ri_entry->priv.key0, ri_entry->priv.key1);

  dbg_info("Inserting the new send buffer request into the request table: %d/%d/%p", proc, request_id, req);
  if (htable_insert(reqtable, (uint64_t)request_id, req) != 0) {
    /* this is bad, we've submitted the request, but we can't track it */
    log_err("Couldn't save request in hashtable");
    goto error_exit;
  }

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
static photonRequest __photon_setup_request_recv(photonAddr addr, int msn, int msize, int bindex, int nbufs, uint64_t request) {
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
  req->mmask = (uint64_t)1<<msn;
  req->length = msize;
  req->bentries[msn] = bindex;
  memcpy(&req->addr, addr, sizeof(*addr));
  
  dbg_info("Inserting the new recv request into the sr table: %lu/0x%016lx/%p",
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
static int __photon_setup_request_send(photonAddr addr, int *bufs, int nbufs, uint32_t request) {
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
  req->mmask = 0x0;
  req->length = 0;
  memcpy(&req->addr, addr, sizeof(*addr));
  memcpy(req->bentries, bufs, MAX_BUF_ENTRIES);
  
  dbg_info("Inserting the new send request into the sr table: 0x%016lx/%u/%p",
           addr->global.proc_id, request, req);
  if (htable_insert(sr_reqtable, (uint64_t)request, req) != 0) {
    /* this is bad, we've submitted the request, but we can't track it */
    log_err("Couldn't save request in hashtable");
    goto error_exit;
  }

  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}

static int _photon_initialized() {
  if (__photon_backend && __photon_config)
    return __photon_backend->initialized();
  else
    return PHOTON_ERROR_NOINIT;
}

static int _photon_init(photonConfig cfg, ProcessInfo *info, photonBI ss) {
  int i, rc;
  char *buf;
  int bufsize, offset;
  int info_ledger_size, FIN_ledger_size;

  srand48(getpid() * time(NULL));

  dbg_info("(nproc %d, rank %d)",_photon_nproc, _photon_myrank);

  INIT_COUNTER(curr_cookie, 1);

  requests = malloc(sizeof(struct photon_req_t) * DEF_NUM_REQUESTS);
  if (!requests) {
    log_err("Failed to allocate request list");
    goto error_exit_req;
  }

  num_requests = DEF_NUM_REQUESTS;
  LIST_INIT(&free_reqs_list);
  SLIST_INIT(&pending_recv_list);

  for(i = 0; i < num_requests; i++) {
    LIST_INSERT_HEAD(&free_reqs_list, &(requests[i]), list);
  }

  dbg_info("create_buffertable()");
  fflush(stderr);
  if (buffertable_init(193)) {
    log_err("Failed to allocate buffer table");
    goto error_exit_req;
  }

  dbg_info("create_reqtable()");
  reqtable = htable_create(193);
  if (!reqtable) {
    log_err("Failed to allocate request table");
    goto error_exit_bt;
  }

  dbg_info("create_ledger_reqtable()");

  ledger_reqtable = htable_create(193);
  if (!ledger_reqtable) {
    log_err("Failed to allocate request table");
    goto error_exit_rt;
  }

  dbg_info("create_sr_reqtable()");

  sr_reqtable = htable_create(193);
  if (!sr_reqtable) {
    log_err("Failed to allocate request table");
    goto error_exit_lrt;
  }
  
  photon_processes = (ProcessInfo *) malloc(sizeof(ProcessInfo) * (_photon_nproc + _photon_nforw));
  if (!photon_processes) {
    log_err("Couldn't allocate process information");
    goto error_exit_srt;
  }

  // Set it to zero, so that we know if it ever got initialized
  memset(photon_processes, 0, sizeof(ProcessInfo) * (_photon_nproc + _photon_nforw));

  dbg_info("alloc'd process info");

  // Everything is x2 cause we need a local and a remote copy of each ledger.
  // Remote Info (_ri_) ledger has an additional x2 cause we need "send-info" and "receive-info" ledgers.
  info_ledger_size = 2 * 2 * sizeof(struct photon_ri_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc + _photon_nforw);
  FIN_ledger_size  = 2 * sizeof(struct photon_rdma_FIN_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc + _photon_nforw);
  bufsize = info_ledger_size + FIN_ledger_size;
  buf = malloc(bufsize);
  if (!buf) {
    log_err("Couldn't allocate ledgers");
    goto error_exit_crb;
  }
  dbg_info("Bufsize: %d", bufsize);

  if (photon_setup_ri_ledgers(photon_processes, buf, LEDGER_SIZE) != 0) {
    log_err("couldn't setup snd/rcv info ledgers");
    goto error_exit_buf;
  }

  // skip 4 ledgers (rcv info local, rcv info remote, snd info local, snd info remote)
  offset = 4 * sizeof(struct photon_ri_ledger_entry_t) * LEDGER_SIZE * (_photon_nproc + _photon_nforw);
  if (photon_setup_FIN_ledger(photon_processes, buf + offset, LEDGER_SIZE) != 0) {
    log_err("couldn't setup send ledgers");
    goto error_exit_buf;
  }

  shared_storage = photon_buffer_create(buf, bufsize);
  if (!shared_storage) {
    log_err("Couldn't register shared storage");
    goto error_exit_buf;
  }

  rc = __photon_backend->init(cfg, photon_processes, shared_storage);
  if (rc != PHOTON_OK) {
    log_err("Could not initialize photon backend");
    goto error_exit_ss;
  }

  // allocate buffers for send/recv operations (after backend initializes)
  uint64_t msgbuf_size, p_size;
  int p_offset, p_hsize;
  if (cfg->use_ud) {
    // we need to ask the backend about the max msg size it can support for UD
    int *mtu, size;
    if (__photon_backend->get_info(photon_processes, PHOTON_ANY_SOURCE, (void**)&mtu, &size, PHOTON_MTU)) {
      dbg_err("Could not get mtu for UD service");
      goto error_exit_ss;
    }
    // gross hack, should ask backend again...
    p_offset = 40;
    p_hsize = sizeof(photon_ud_hdr);
    p_size = p_offset + *mtu - 84;
  }
  else {
    p_offset = 0;
    p_hsize = 0;
    p_size = p_offset + SMSG_SIZE;
  }
  dbg_info("sr partition size: %lu", p_size);

  // create enough space to accomodate every rank sending LEDGER_SIZE messages
  msgbuf_size = LEDGER_SIZE * p_size * _photon_nproc;
  
  sendbuf = photon_msgbuffer_new(msgbuf_size, p_size, p_offset, p_hsize);
  if (!sendbuf) {
    dbg_err("could not create send message buffer");
    goto error_exit_ss;
  }
  photon_buffer_register(sendbuf->db, __photon_backend->context);

  recvbuf = photon_msgbuffer_new(msgbuf_size, p_size, p_offset, p_hsize);
  if (!recvbuf) {
    dbg_err("could not create recv message buffer");
    goto error_exit_sb;
  }
  photon_buffer_register(recvbuf->db, __photon_backend->context);

  // pre-post the receive buffers when UD service is requested
  if (cfg->use_ud) {
    for (i = 0; i < recvbuf->p_count; i++) {
      __photon_backend->rdma_recv(NULL, (uintptr_t)recvbuf->entries[i].base, recvbuf->p_size,
                                  &recvbuf->db->buf, (( (uint64_t)REQUEST_COOK_RECV) << 32) | i);
    }
  }

  // register any buffers that were requested before init
  while( !SLIST_EMPTY(&pending_mem_register_list) ) {
    struct photon_mem_register_req *mem_reg_req;
    dbg_info("registering buffer in queue");
    mem_reg_req = SLIST_FIRST(&pending_mem_register_list);
    SLIST_REMOVE_HEAD(&pending_mem_register_list, list);
    photon_register_buffer(mem_reg_req->buffer, mem_reg_req->buffer_size);
  }

#ifdef PHOTON_MULTITHREADED
  if (pthread_create(&ledger_watcher, NULL, __photon_req_watcher, NULL)) {
    log_err("pthread_create() failed");
    goto error_exit_rb;
  }
#endif

  dbg_info("ended successfully =============");

  return PHOTON_OK;

#ifdef PHOTON_MULTITHREADED  
 error_exit_rb:
  photon_msgbuffer_free(recvbuf);
#endif
 error_exit_sb:
  photon_msgbuffer_free(sendbuf);
 error_exit_ss:
  photon_buffer_free(shared_storage);
 error_exit_buf:
  if (buf)
    free(buf);
 error_exit_crb:
  free(photon_processes);
 error_exit_srt:
  htable_free(sr_reqtable);
 error_exit_lrt:
  htable_free(ledger_reqtable);
 error_exit_rt:
  htable_free(reqtable);
 error_exit_bt:
  buffertable_finalize();
 error_exit_req:
  free(requests);
  DESTROY_COUNTER(curr_cookie);
  
  return PHOTON_ERROR;
}

static int _photon_finalize() {
  int rc;

  rc = __photon_backend->finalize();
  if (rc != PHOTON_OK) {
    log_err("Could not finalize backend");
    return PHOTON_ERROR;
  }
  return PHOTON_OK;
}

static int _photon_register_buffer(void *buffer, uint64_t size) {
  static int first_time = 1;
  photonBI db;

  dbg_info("(%p, %lu)", buffer, size);

  if(__photon_backend->initialized() != PHOTON_OK) {
    struct photon_mem_register_req *mem_reg_req;
    if( first_time ) {
      SLIST_INIT(&pending_mem_register_list);
      first_time = 0;
    }
    mem_reg_req = malloc( sizeof(struct photon_mem_register_req) );
    mem_reg_req->buffer = buffer;
    mem_reg_req->buffer_size = size;

    SLIST_INSERT_HEAD(&pending_mem_register_list, mem_reg_req, list);
    dbg_info("called before init, queueing buffer info");
    goto normal_exit;
  }

  if (buffertable_find_exact(buffer, size, &db) == 0) {
    dbg_info("we had an existing buffer, reusing it");
    db->ref_count++;
    goto normal_exit;
  }

  db = photon_buffer_create(buffer, size);
  if (!db) {
    log_err("could not create photon buffer");
    goto error_exit;
  }

  dbg_info("created buffer: %p", db);

  if (photon_buffer_register(db, __photon_backend->context) != 0) {
    log_err("Couldn't register buffer");
    goto error_exit_db;
  }

  dbg_info("registered buffer");

  if (buffertable_insert(db) != 0) {
    goto error_exit_db;
  }

  dbg_info("added buffer to hash table");

normal_exit:
  return PHOTON_OK;
error_exit_db:
  photon_buffer_free(db);
error_exit:
  return PHOTON_ERROR;
}

static int _photon_unregister_buffer(void *buffer, uint64_t size) {
  photonBI db;

  dbg_info();

  if(__photon_backend->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  if (buffertable_find_exact(buffer, size, &db) != 0) {
    dbg_info("no such buffer is registered");
    goto error_exit;
  }

  if (--(db->ref_count) == 0) {
    if (photon_buffer_unregister(db, __photon_backend->context) != 0) {
      goto error_exit;
    }
    buffertable_remove(db);
    photon_buffer_free(db);
  }

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

///////////////////////////////////////////////////////////////////////////////
// __photon_nbpop_event() is non blocking and returns:
// -1 if an error occured.
//	0 if the request (req) specified in the argument has completed.
//	1 if either no event was in the queue, or there was an event but not for the specified request (req).
static int __photon_nbpop_event(photonRequest req) {
  int rc;
  
  dbg_info("(%lu)",req->id);

  if(req->state == REQUEST_PENDING) {
    uint32_t cookie;
    photon_event_status event;

    rc = __photon_backend->get_event(&event);
    if (rc != PHOTON_OK) {
      dbg_err("Could not get event");
      goto error_exit;
    }

    cookie = (uint32_t)( (event.id<<32)>>32);
    if (cookie == req->id) {
      dbg_info("setting request completed with cookie:%u", cookie);
      req->state = REQUEST_COMPLETED;
    }
    /* handle any other request completions we might get from the backend event queue */
    else if (cookie != NULL_COOKIE) {
      photonRequest tmp_req;
      if (htable_lookup(reqtable, (uint64_t)cookie, (void**)&tmp_req) == 0) {
        dbg_info("setting request completed with cookie:%u", cookie);
        tmp_req->state = REQUEST_COMPLETED;
      }
    }
  }
  
  /* clean up a completed request if the FIN has already been sent
     otherwise, we clean up in send_FIN() */
  if ((req->state == REQUEST_COMPLETED) && (req->flags & REQUEST_FLAG_FIN)) {
    dbg_info("Removing request %lu for remote buffer request", req->id);
    htable_remove(reqtable, (uint64_t)req->id, NULL);
    SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
    dbg_info("%d requests left in reqtable", htable_count(reqtable));
  } 
  
  dbg_info("returning %d", (req->state == REQUEST_COMPLETED)?0:1 );
  return (req->state == REQUEST_COMPLETED)?0:1;

error_exit:
  return -1;
}

static int __photon_handle_send_event(photonRequest req, uint64_t id) {
  photonRequest creq = NULL;
  uint32_t cookie;
  int i;

  dbg_info("handling send completion with id: 0x%016lx", id);

  cookie = (uint32_t)((id<<32)>>32);
    
  if(req && (cookie == req->id)) {
    creq = req;
  }
  else if (cookie != NULL_COOKIE) {
    photonRequest tmp_req;
    if (htable_lookup(sr_reqtable, (uint64_t)cookie, (void**)&tmp_req) == 0) {
      creq = tmp_req;
    }
  }
  
  // we have a request to update
  if (creq) {
    if (creq->state == REQUEST_PENDING) {
      uint16_t msn;
      msn = (uint16_t)((id<<16)>>48);
      req->mmask |= ((uint64_t)1<<msn);
      if (!( req->mmask ^ ~(~(uint64_t)0<<req->num_entries))) {
        // additional condition would be ACK from receiver
        creq->state = REQUEST_COMPLETED;
        // mark sendbuf entries as available again
        for (i=0; i<req->num_entries; i++) {
          photon_msgbuffer_free_entry(sendbuf, req->bentries[i]);
        }
      }
    }
    else {
      dbg_warn("not updating request that's not pending");
    }
  }
  else {
    dbg_err("could not find matching request for id: 0x%016lx", id);
    return PHOTON_ERROR;
  }

  return PHOTON_OK;
}

static int __photon_handle_recv_event(uint64_t id) {
  photonRequest req;
  photon_ud_hdr *hdr;
  uint64_t cookie;
  uint32_t bindex;
  
  dbg_info("handling recv completion with id: 0x%016lx", id);

  bindex = (uint32_t)((id<<32)>>32);
  
  // TODO: get backend gid...not really needed

  hdr = (photon_ud_hdr*)recvbuf->entries[bindex].hptr;
  dbg_info("r_request: %u", hdr->request);
  dbg_info("src      : %u", hdr->src_addr);
  dbg_info("length   : %u", hdr->length);
  dbg_info("msn      : %d", hdr->msn);
  dbg_info("nmsg     : %d", hdr->nmsg);

  if (hdr->src_addr == _photon_myrank) {
    // we somehow sent to our own rank, just reset mbuf entry and return
    __photon_backend->rdma_recv(NULL, (uintptr_t)recvbuf->entries[bindex].base,
                                recvbuf->p_size, &recvbuf->db->buf,
                                (( (uint64_t)REQUEST_COOK_RECV) << 32) | bindex);
    photon_msgbuffer_free_entry(recvbuf, bindex);
    return PHOTON_OK;
  }

  // construct a cookie to query the htable with
  // |..~remote proc...|...remote r_id...|
  cookie = ((uint64_t)(~hdr->src_addr)<<32) | hdr->request;
  if (htable_lookup(sr_reqtable, cookie, (void**)&req) == 0) {
    // update existing req
    req->mmask |= ((uint64_t)1<<hdr->msn);
    req->length += hdr->length;
    req->bentries[hdr->msn] = bindex;
  }
  else {
    // create a new request for the message
    photon_addr addr = {.global.proc_id = hdr->src_addr};
    req = __photon_setup_request_recv(&addr, hdr->msn, hdr->length, bindex, hdr->nmsg, cookie);
  }
  
  // now check if we have the whole message
  // if so, add to pending recv list and remove from htable
  if (req) {
    if (!( req->mmask ^ ~(~(uint64_t)0<<req->num_entries))) {
      dbg_info("adding recv request to pending recv list: %lu/0x%016lx", req->id, cookie);
      SAFE_SLIST_INSERT_HEAD(&pending_recv_list, req, slist);
      req->state = REQUEST_COMPLETED;
      // send an ACK back to sender here
    }
  }
  else {
    dbg_err("request creation failed");
  }

  return PHOTON_OK;
}

static int __photon_nbpop_sr(photonRequest req) {
  int rc;

  if (req) {
    dbg_info("(%lu)", req->id);
  }
  else {
    dbg_info("(probing)");
  }

  if (!req || (req && req->state == REQUEST_PENDING)) {

    uint32_t prefix;
    photon_event_status event;
    
    rc = __photon_backend->get_event(&event);
    if (rc < 0) {
      dbg_err("Error getting event, rc=%d", rc);
      goto error_exit;
    }
    else if (rc != PHOTON_OK) {
      return 1;
    }

    prefix = (uint32_t)(event.id>>32);
    if (prefix == REQUEST_COOK_RECV) {
      __photon_handle_recv_event(event.id);
    }
    else {
      __photon_handle_send_event(req, event.id);
    }    
  }

  if (req && req->state == REQUEST_COMPLETED) {
    dbg_info("Removing request 0x%016lx for send/recv", req->id);
    htable_remove(sr_reqtable, req->id, NULL);
    SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
    dbg_info("%d requests left in sr_reqtable", htable_count(sr_reqtable));
  } 

  if (req) {
    dbg_info("returning %d", (req->state == REQUEST_COMPLETED)?0:1 );
    return (req->state == REQUEST_COMPLETED)?0:1;
  }
  else {
    return 1;
  }

 error_exit:
  return -1;
}

///////////////////////////////////////////////////////////////////////////////
// returns
// -1 if an error occured.
//	0 if the FIN associated with "req" was found and poped, or
//		the "req" is not pending.	 This is not an error, if a previous
//		call to __photon_nbpop_ledger() poped the FIN that corresponds to "req".
//	1 if the request is pending and the FIN has not arrived yet
static int __photon_nbpop_ledger(photonRequest req) {
  int num, new_curr;
  int curr, i=-1;

  dbg_info("(%lu)", req->id);

  if(req->state == REQUEST_PENDING) {
    
    // Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
    for(i = 0; i < _photon_nproc; i++) {
      photonFINLedgerEntry curr_entry;
      curr = photon_processes[i].local_FIN_ledger->curr;
      curr_entry = &(photon_processes[i].local_FIN_ledger->entries[curr]);
      if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
        dbg_info("Found curr: %d req: %u while looking for req: %lu", curr, curr_entry->request, req->id);
        curr_entry->header = 0;
        curr_entry->footer = 0;

        if (curr_entry->request == req->id) {
          req->state = REQUEST_COMPLETED;
        }
        /* handle any other request completions we might get from the backend event queue */
        else {
          photonRequest tmp_req;
          if (htable_lookup(ledger_reqtable, (uint64_t)curr_entry->request, (void**)&tmp_req) == 0) {
            dbg_info("setting request completed with cookie:%u", curr_entry->request);
            tmp_req->state = REQUEST_COMPLETED;
          }
        }
        num = photon_processes[i].local_FIN_ledger->num_entries;
        new_curr = (photon_processes[i].local_FIN_ledger->curr + 1) % num;
        photon_processes[i].local_FIN_ledger->curr = new_curr;
      }
    } 
  }
  
  if (req->state == REQUEST_COMPLETED) {
    dbg_info("removing RDMA i: %u req: %lu", i, req->id);
    htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
    SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
  }
  
  if ((req->state != REQUEST_COMPLETED) && (req->state != REQUEST_PENDING)) {
    dbg_info("req->state != (PENDING | COMPLETE), returning 0");
    return 0;
  }
  
  dbg_info("at end, returning %d", (req->state == REQUEST_COMPLETED)?0:1);
  return (req->state == REQUEST_COMPLETED)?0:1;
}

static int __photon_wait_ledger(photonRequest req) {
  void *test;
  int curr, num_entries, i=-1;

  dbg_info("(%lu)",req->id);

#ifdef DEBUG
  for(i = 0; i < _photon_nproc; i++) {
    photonFINLedgerEntry curr_entry;
    curr = photon_processes[i].local_FIN_ledger->curr;
    curr_entry = &(photon_processes[i].local_FIN_ledger->entries[curr]);
    dbg_info("curr_entry(proc==%d)=%p",i,curr_entry);
  }
#endif
  while (req->state == REQUEST_PENDING) {

    // Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
    for(i = 0; i < _photon_nproc; i++) {
      photonFINLedgerEntry curr_entry;
      curr = photon_processes[i].local_FIN_ledger->curr;
      curr_entry = &(photon_processes[i].local_FIN_ledger->entries[curr]);
      if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
        dbg_info("Found: %d/%u/%lu", curr, curr_entry->request, req->id);
        curr_entry->header = 0;
        curr_entry->footer = 0;

        if (curr_entry->request == req->id) {
          req->state = REQUEST_COMPLETED;
        }
        else {
          photonRequest tmp_req;
          
          if (htable_lookup(ledger_reqtable, (uint64_t)curr_entry->request, &test) == 0) {
            tmp_req = test;
            tmp_req->state = REQUEST_COMPLETED;
          }
        }

        num_entries = photon_processes[i].local_FIN_ledger->num_entries;
        curr = photon_processes[i].local_FIN_ledger->curr;
        curr = (curr + 1) % num_entries;
        photon_processes[i].local_FIN_ledger->curr = curr;
      }
    }
  }
  dbg_info("removing RDMA: %u/%lu", i, req->id);
  htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
  SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
  dbg_info("%d requests left in reqtable", htable_count(ledger_reqtable));

  return (req->state == REQUEST_COMPLETED)?0:-1;
}

static int __photon_wait_event(photonRequest req) {
  int rc;
  
  while (req->state == REQUEST_PENDING) {
    uint32_t cookie;
    photon_event_status event;

    rc = __photon_backend->get_event(&event);
    if (rc != PHOTON_OK) {
      dbg_err("Could not get event");
      goto error_exit;
    }
    
    cookie = (uint32_t)( (event.id<<32)>>32);
    if (cookie == req->id) {
      req->state = REQUEST_COMPLETED;
      dbg_info("setting request complete with cookie:%u", cookie);
    }
  }

  return (req->state == REQUEST_COMPLETED)?0:-1;

error_exit:
  return -1;
}

///////////////////////////////////////////////////////////////////////////////
// photon_test() is a nonblocking operation that checks the event queue to see if
// the event associated with the "request" parameter has completed.	 It returns:
//	0 if the event associated with "request" was in the queue and was successfully poped.
//	1 if "request" was not in the request tables.	 This is not an error if photon_test()
//		is called in a loop and is called multiple times for each request.
//     -1 if an error occured.
//
// When photon_test() returns zero (success) the "flag" parameter has the value:
//	0 if the event that was poped does not correspond to "request", or if none of the operations completed.
//	1 if the event that was poped does correspond to "request".
//
//	When photon_test() returns 0 and flag==0 the "status" structure is also filled
//	unless the constant "MPI_STATUS_IGNORE" was passed as the "status" argument.
//
// Regardless of the return value and the value of "flag", the parameter "type"
// will be set to 0 (zero) when the request is of type event and 1 (one) when the
// request is of type ledger.  type is set to 2 when the request was a send/recv
static int _photon_test(uint32_t request, int *flag, int *type, photonStatus status) {
  photonRequest req;
  void *test;
  int ret_val;

  dbg_info("(%d)",request);

  if (htable_lookup(reqtable, (uint64_t)request, &test) != 0) {
    if (htable_lookup(ledger_reqtable, (uint64_t)request, &test) != 0) {
      if (htable_lookup(sr_reqtable, (uint64_t)request, &test) != 0) {
        dbg_info("Request is not in any request-table");
        // Unlike photon_wait(), we might call photon_test() multiple times on a request,
        // e.g., in an unguarded loop.	flag==-1 will signify that the operation is
        // not pending.	 This means, it might be completed, it might have never been
        // issued.	It's up to the application to guarantee correctness, by keeping
        // track, of	what's going on.	Unless you know what you are doing, consider
        // (flag==-1 && return_value==1) to be an error case.
        dbg_info("returning 1, flag:-1");
        *flag = -1;
        return 1;
      }
    }
  }

  req = test;

  *flag = 0;

#ifdef PHOTON_MULTITHREADED
  pthread_mutex_lock(&req->mtx);
  {
    ret_val = (req->state == REQUEST_COMPLETED)?0:1;
  }
  pthread_mutex_unlock(&req->mtx);
#else
  switch (req->type) {
  case LEDGER:
    {
      if( type != NULL ) *type = 1;
      ret_val = __photon_nbpop_ledger(req);
    }
    break;
  case SENDRECV:
    {
      if( type != NULL ) *type = 2;
      ret_val = __photon_nbpop_sr(req);
    }
    break;
  default:
    {
      if( type != NULL ) *type = 0;
      ret_val = __photon_nbpop_event(req);
    }
    break;
  }
#endif

  if( !ret_val ) {
    *flag = 1;
    status->src_addr.global.proc_id = req->proc;
    status->tag = req->tag;
    status->count = 1;
    status->error = 0;
    dbg_info("returning 0, flag:1");
    return 0;
  }
  else if( ret_val > 0 ) {
    dbg_info("returning 0, flag:0");
    *flag = 0;
    return 0;
  }
  else {
    dbg_info("returning -1, flag:0");
    *flag = 0;
    return -1;
  }
}

static int _photon_wait(uint32_t request) {
  photonRequest req;

  dbg_info("(%d)",request);

  if (htable_lookup(reqtable, (uint64_t)request, (void**)&req) != 0) {
    if (htable_lookup(ledger_reqtable, (uint64_t)request, (void**)&req) != 0) {
      log_err("Wrong request value, operation not in table");
      return -1;
    }
  }

#ifdef PHOTON_MULTITHREADED

  pthread_mutex_lock(&req->mtx);
  {
    while(req->state == REQUEST_PENDING)
      pthread_cond_wait(&req->completed, &req->mtx);

    if (req->type == LEDGER) {
      if (htable_lookup(ledger_reqtable, (uint64_t)req->id, NULL) != -1) {
        dbg_info("removing ledger RDMA: %u", req->id);
        htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
        SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
        dbg_info("%d requests left in ledgertable", htable_count(ledger_reqtable));
      }
    }
    else {
      if (htable_lookup(reqtable, (uint64_t)req->id, NULL) != -1) {
        dbg_info("removing event with cookie:%u", req->id);
        htable_remove(reqtable, (uint64_t)req->id, NULL);
        SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
        dbg_info("%d requests left in reqtable", htable_count(reqtable));
      }
    }
  }
  pthread_mutex_unlock(&req->mtx);

  return (req->state == REQUEST_COMPLETED)?0:-1;

#else
  if (req->type == LEDGER)
    return __photon_wait_ledger(req);
  else
    return __photon_wait_event(req);
#endif
}

static int _photon_send(photonAddr addr, void *ptr, uint64_t size, int flags, uint64_t *request) {
  //char buf[40];
  //inet_ntop(AF_INET6, addr->raw, buf, 40);
  //dbg_info("(%s, %p, %lu, %d)", buf, ptr, size, flags);

  photon_addr saddr;
  int bufs[MAX_BUF_ENTRIES];
  uint32_t request_id;
  uint64_t cookie;
  uint64_t bytes_remaining, bytes_sent, send_bytes;
  uintptr_t buf_addr;
  int rc, m_count, num_msgs;

  rc = _photon_handle_addr(addr, &saddr);
  if (rc != PHOTON_OK) {
    goto error_exit;
  }

  request_id = INC_COUNTER(curr_cookie);
  
  // segment and send as entries of the sendbuf
  bytes_remaining = size;
  bytes_sent = 0;
  m_count = 0;
  
  num_msgs = size / sendbuf->m_size + 1;
  if (num_msgs > MAX_BUF_ENTRIES) {
    dbg_err("Message of size %lu requires too many mbuf entries, %u, max=%u", size, num_msgs, MAX_BUF_ENTRIES);
    goto error_exit;
  }

  do {
    photon_mbe *bentry;
    int b_ind;
    bentry = photon_msgbuffer_get_entry(sendbuf, &b_ind);
    
    // build a unique id that allows us to track the send requests
    // 0xcafeXXXXYYYYYYYY
    // XXXX     = message sequence number
    // YYYYYYYY = request id 
    cookie = (((uint64_t)REQUEST_COOK_SEND)<<48) | request_id;
    cookie |= (((uint64_t)m_count)<<32);

    if (bytes_remaining > sendbuf->m_size) {
      send_bytes = sendbuf->m_size;
    }
    else {
      send_bytes = bytes_remaining;
    }

    // copy data into one message
    memcpy(bentry->mptr, ptr + bytes_sent, send_bytes);
    
    if (__photon_config->use_ud) {
      // create the header
      photon_ud_hdr *hdr = (photon_ud_hdr*)bentry->hptr;
      hdr->request = request_id;
      hdr->src_addr = (uint32_t)_photon_myrank;
      hdr->length = send_bytes;
      hdr->msn = m_count;
      hdr->nmsg = num_msgs;
    }

    dbg_info("sending mbuf [%d/%d], size=%lu, header size=%d", m_count, num_msgs-1, send_bytes, sendbuf->p_hsize);

    buf_addr = (uintptr_t)bentry->hptr;
    rc = __photon_backend->rdma_send(&saddr, buf_addr, send_bytes + sendbuf->p_hsize, &sendbuf->db->buf, cookie);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA SEND failed for 0x%016lx\n", cookie);
      goto error_exit;
    }
    
    bufs[m_count++] = b_ind;
    bytes_sent += send_bytes;
    bytes_remaining -= send_bytes;
  } while (bytes_remaining);
  
  if (request != NULL) {
    *request = (uint64_t)request_id;
    
    rc = __photon_setup_request_send(addr, bufs, m_count, request_id);
    if (rc != PHOTON_OK) {
      dbg_info("Could not setup sendrecv request");
      goto error_exit;
    }
  }  
  
  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

static int _photon_recv(uint64_t request, void *ptr, uint64_t size, int flags) {
  photonRequest req;

  dbg_info("(0x%016lx, %p, %lu, %d)", request, ptr, size, flags);
  
  if (htable_lookup(sr_reqtable, request, (void**)&req) == 0) {
    if (request != req->id) {
      dbg_err("request id mismatch!");
      goto error_exit;
    }
    
    uint64_t bytes_remaining;
    uint64_t bytes_copied;
    uint64_t copy_bytes;
    
    int rc, m_count, bind;

    bytes_remaining = size;
    bytes_copied = 0;
    m_count = 0;
    while (bytes_remaining) {
      if (bytes_remaining > recvbuf->m_size) {
        copy_bytes = recvbuf->m_size;
      }
      else {
        copy_bytes = bytes_remaining;
      }

      bind = req->bentries[m_count];
      memcpy(ptr + bytes_copied, recvbuf->entries[bind].mptr, copy_bytes);

      // re-arm this buffer entry for another recv
      rc = __photon_backend->rdma_recv(NULL, (uintptr_t)recvbuf->entries[bind].base, recvbuf->p_size,
                                       &recvbuf->db->buf, (( (uint64_t)REQUEST_COOK_RECV) << 32) | bind);
      if (rc != PHOTON_OK) {
        dbg_err("could not post_recv() buffer entry");
        goto error_exit;
      }

      m_count++;
      bytes_copied += copy_bytes;
      bytes_remaining -= copy_bytes;
    }

    dbg_info("removing recv request from sr_reqtable: 0x%016lx", request);
    htable_remove(sr_reqtable, request, NULL);
  }
  else {
    dbg_info("request not found in sr_reqtable");
    goto error_exit;
  }

  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

static int _photon_post_recv_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, uint32_t *request) {
  photonBI db;
  uint64_t cookie;
  photonRILedgerEntry entry;
  int curr, num_entries, rc;
  uint32_t request_id;

  dbg_info("(%d, %p, %lu, %d, %p)", proc, ptr, size, tag, request);
  
  if (buffertable_find_containing( (void *) ptr, (int)size, &db) != 0) {
    log_err("Requested recv from ptr not in table");
    goto error_exit;
  }

  request_id = INC_COUNTER(curr_cookie);
  dbg_info("Incrementing curr_cookie_count to: %d", request_id);

  /* proc == -1 means ANY_SOURCE.  In this case all potential senders must post a send request
     which will write into our snd_info ledger entries such that:
     rkey == 0
     addr == (uintptr_t)0  */
  if( proc == -1 ) {
    proc = photon_wait_send_request_rdma(tag);
  }

  curr = photon_processes[proc].remote_rcv_info_ledger->curr;
  entry = &photon_processes[proc].remote_rcv_info_ledger->entries[curr];

  /* fill in what we're going to transfer */
  entry->header = 1;
  entry->request = request_id;
  entry->tag = tag;
  entry->addr = (uintptr_t) ptr;
  entry->size = size;
  entry->priv = db->buf.priv;
  entry->footer = 1;

  dbg_info("Post recv");
  dbg_info("Request: %u", entry->request);
  dbg_info("Address: %p", (void *)entry->addr);
  dbg_info("Size: %lu", entry->size);
  dbg_info("Tag: %d", entry->tag);
  dbg_info("Keys: 0x%016lx / 0x%016lx", entry->priv.key0, entry->priv.key1);
  
  {
    uintptr_t rmt_addr;
    rmt_addr  = photon_processes[proc].remote_rcv_info_ledger->remote.addr;
    rmt_addr += photon_processes[proc].remote_rcv_info_ledger->curr * sizeof(*entry);
    cookie = (( (uint64_t)proc)<<32) | NULL_COOKIE;

    rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
                                    &(photon_processes[proc].remote_rcv_info_ledger->remote), cookie);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT failed for %lu\n", cookie);
      goto error_exit;
    }
  }

  if (request != NULL) {
    photonRequest req;

    req = __photon_get_request();
    if (!req) {
      log_err("Couldn't allocate request\n");
      goto error_exit;
    }
    req->id = request_id;
    req->state = REQUEST_PENDING;
    // photon_post_recv_buffer_rdma() initiates a receiver initiated handshake.	For this reason,
    // we don't care when the function is completed, but rather when the transfer associated with
    // this handshake is completed.	 This will be reflected in the LEDGER by the corresponding
    // photon_send_FIN() posted by the sender.
    req->type = LEDGER;
    req->proc = proc;
    req->tag = tag;

    dbg_info("Inserting the RDMA request into the request table: %d/%p", request_id, req);
    if (htable_insert(ledger_reqtable, (uint64_t)request_id, req) != 0) {
      // this is bad, we've submitted the request, but we can't track it
      log_err("Couldn't save request in hashtable");
    }
    *request = request_id;
  }

  num_entries = photon_processes[proc].remote_rcv_info_ledger->num_entries;
  curr = photon_processes[proc].remote_rcv_info_ledger->curr;
  curr = (curr + 1) % num_entries;
  photon_processes[proc].remote_rcv_info_ledger->curr = curr;

  dbg_info("New curr (proc=%d): %u", proc, photon_processes[proc].remote_rcv_info_ledger->curr);

  return PHOTON_OK;

error_exit:
  if (request != NULL) {
    *request = NULL_COOKIE;
  }
  return PHOTON_ERROR;
}

static int _photon_post_send_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, uint32_t *request) {
  photonBI db;
  photonRILedgerEntry entry;
  int curr, num_entries, rc;
  uint64_t cookie;
  uint32_t request_id;

  dbg_info("(%d, %p, %lu, %d, %p)", proc, ptr, size, tag, request);

  if (buffertable_find_containing( (void*)ptr, (int)size, &db) != 0) {
    log_err("Requested post of send buffer for ptr not in table");
    goto error_exit;
  }

  request_id = INC_COUNTER(curr_cookie);
  dbg_info("Incrementing curr_cookie_count to: %d", request_id);

  curr = photon_processes[proc].remote_snd_info_ledger->curr;
  entry = &photon_processes[proc].remote_snd_info_ledger->entries[curr];

  // fill in what we're going to transfer
  entry->header = 1;
  entry->request = request_id;
  entry->tag = tag;
  entry->addr = (uintptr_t)ptr;
  entry->size = size;
  entry->priv = db->buf.priv;
  entry->footer = 1;

  dbg_info("Post send request");
  dbg_info("Request: %u", entry->request);
  dbg_info("Addr: %p", (void *)entry->addr);
  dbg_info("Size: %lu", entry->size);
  dbg_info("Tag: %d", entry->tag);
  dbg_info("Keys: 0x%016lx / 0x%016lx", entry->priv.key0, entry->priv.key1);
  
  {
    uintptr_t rmt_addr;
    rmt_addr  = photon_processes[proc].remote_snd_info_ledger->remote.addr;
    rmt_addr += photon_processes[proc].remote_snd_info_ledger->curr * sizeof(*entry);
    cookie = (( (uint64_t)proc)<<32) | request_id;

    rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
                                    &(photon_processes[proc].remote_rcv_info_ledger->remote), cookie);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT failed for %lu\n", cookie);
      goto error_exit;
    }
  }

  if (request != NULL) {
    photonRequest req;

    req = __photon_get_request();
    if (!req) {
      log_err("Couldn't allocate request\n");
      goto error_exit;
    }
    req->id = request_id;
    req->state = REQUEST_PENDING;
    // photon_post_send_buffer_rdma() initiates a sender initiated handshake.	For this reason,
    // we don't care when the function is completed, but rather when the transfer associated with
    // this handshake is completed.	 This will be reflected in the LEDGER by the corresponding
    // photon_send_FIN() posted by the receiver.
    req->type = LEDGER;
    req->proc = proc;
    req->tag = tag;
    
    dbg_info("Inserting the RDMA request into the request table: %d/%p", request_id, req);
    if (htable_insert(ledger_reqtable, (uint64_t)request_id, req) != 0) {
      // this is bad, we've submitted the request, but we can't track it
      log_err("Couldn't save request in hashtable");
    }
    *request = request_id;
  }

  num_entries = photon_processes[proc].remote_snd_info_ledger->num_entries;
  curr = photon_processes[proc].remote_snd_info_ledger->curr;
  curr = (curr + 1) % num_entries;
  photon_processes[proc].remote_snd_info_ledger->curr = curr;
  dbg_info("New curr: %u", curr);

  return PHOTON_OK;

error_exit:
  if (request != NULL) {
    *request = NULL_COOKIE;
  }
  return PHOTON_ERROR;
}

static int _photon_post_send_request_rdma(int proc, uint64_t size, int tag, uint32_t *request) {
  photonRILedgerEntry entry;
  int curr, num_entries, rc;
  uint64_t cookie;
  uint32_t request_id;

  dbg_info("(%d, %lu, %d, %p)", proc, size, tag, request);

  request_id = INC_COUNTER(curr_cookie);
  dbg_info("Incrementing curr_cookie_count to: %d", request_id);

  curr = photon_processes[proc].remote_snd_info_ledger->curr;
  entry = &photon_processes[proc].remote_snd_info_ledger->entries[curr];

  /* fill in what we're going to transfer
     this is just an intent to transfer, no real info */
  entry->header = 1;
  entry->request = request_id;
  entry->tag = tag;
  entry->addr = (uintptr_t)0;
  entry->size = size;
  entry->priv = (struct photon_buffer_priv_t){0, 0};
  entry->footer = 1;

  dbg_info("Post send request");
  dbg_info("Request: %u", entry->request);
  dbg_info("Addr: %p", (void *)entry->addr);
  dbg_info("Size: %lu", entry->size);
  dbg_info("Tag: %d", entry->tag);
  dbg_info("Keys: 0x%016lx / 0x%016lx", entry->priv.key0, entry->priv.key1);

  {
    uintptr_t rmt_addr;
    rmt_addr  = photon_processes[proc].remote_snd_info_ledger->remote.addr;
    rmt_addr += photon_processes[proc].remote_snd_info_ledger->curr * sizeof(*entry);
    cookie = (( (uint64_t)proc)<<32) | request_id;

    rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
                                    &(photon_processes[proc].remote_rcv_info_ledger->remote), cookie);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT failed for %lu\n", cookie);
      goto error_exit;
    }
  }

  if (request != NULL) {
    photonRequest req;

    req = __photon_get_request();
    if (!req) {
      log_err("Couldn't allocate request\n");
      goto error_exit;
    }
    req->id = request_id;
    req->state = REQUEST_PENDING;
    // photon_post_send_request_rdma() causes an RDMA transfer, but its own completion is
    // communicated to the task that posts it through a DTO completion event.	 This
    // function informs the receiver about an upcoming send, it does NOT initiate
    // a data transfer handshake and that's why it's not a LEDGER event.
    req->type = EVQUEUE;
    req->proc = proc;
    req->tag = tag;

    dbg_info("Inserting the RDMA request into the request table: %d/%p", request_id, req);
    if (htable_insert(reqtable, (uint64_t)request_id, req) != 0) {
      // this is bad, we've submitted the request, but we can't track it
      log_err("Couldn't save request in hashtable");
    }
    *request = request_id;
  }

  num_entries = photon_processes[proc].remote_snd_info_ledger->num_entries;
  curr = photon_processes[proc].remote_snd_info_ledger->curr;
  curr = (curr + 1) % num_entries;
  photon_processes[proc].remote_snd_info_ledger->curr = curr;
  dbg_info("New curr: %u", curr);

  return PHOTON_OK;

error_exit:
  if (request != NULL) {
    *request = NULL_COOKIE;
  }
  return PHOTON_ERROR;
}

static int _photon_wait_recv_buffer_rdma(int proc, int tag, uint32_t *request) {
  photonRILedgerEntry curr_entry, entry_iterator;
  struct photon_ri_ledger_entry_t tmp_entry;
  int ret, count, curr, num_entries, still_searching;

  dbg_info("(%d, %d)", proc, tag);
  dbg_info("Spinning on info ledger looking for receive request");
  dbg_info("curr == %d", photon_processes[proc].local_rcv_info_ledger->curr);

  curr = photon_processes[proc].local_rcv_info_ledger->curr;
  curr_entry = &(photon_processes[proc].local_rcv_info_ledger->entries[curr]);

  dbg_info("looking in position %d/%p", photon_processes[proc].local_rcv_info_ledger->curr, curr_entry);

  count = 1;
  still_searching = 1;
  entry_iterator = curr_entry;
  do {
    while (entry_iterator->header == 0 || entry_iterator->footer == 0) {
      ;
    }
    if( (tag < 0) || (entry_iterator->tag == tag ) ) {
      still_searching = 0;
    }
    else {
      curr = photon_processes[proc].local_rcv_info_ledger->curr;
      num_entries = photon_processes[proc].local_rcv_info_ledger->num_entries;
      curr = (curr + count++) % num_entries;
      entry_iterator = &(photon_processes[proc].local_rcv_info_ledger->entries[curr]);
    }
  }
  while(still_searching);

  /* If it wasn't the first pending receive request, swap the one we will serve ( entry_iterator) with
     the first pending (curr_entry) in the info ledger, so that we can increment the current pointer
     (photon_processes[proc].local_rcv_info_ledger->curr) and skip the request we will serve without losing any
     pending requests. */
  if( entry_iterator != curr_entry ) {
    tmp_entry = *entry_iterator;
    *entry_iterator = *curr_entry;
    *curr_entry = tmp_entry;
  }

  curr_entry->header = 0;
  curr_entry->footer = 0;

  if (request != NULL) {
    ret = __photon_setup_request_ledger(curr_entry, proc, request);
    if (ret != PHOTON_OK) {
      log_err("Could not setup request");
      goto error_exit;
    }
  }

  num_entries = photon_processes[proc].local_rcv_info_ledger->num_entries;
  curr = photon_processes[proc].local_rcv_info_ledger->curr;
  curr = (curr + 1) % num_entries;
  photon_processes[proc].local_rcv_info_ledger->curr = curr;

  dbg_info("new curr == %d", photon_processes[proc].local_rcv_info_ledger->curr);
  
  return PHOTON_OK;
 error_exit:
  return PHOTON_ERROR;
}

static int _photon_wait_send_buffer_rdma(int proc, int tag, uint32_t *request) {
  photonRILedgerEntry curr_entry, entry_iterator;
  struct photon_ri_ledger_entry_t tmp_entry;
  int ret, count, curr, num_entries, still_searching;

  dbg_info("(%d, %d)", proc, tag);

  curr = photon_processes[proc].local_snd_info_ledger->curr;
  curr_entry = &(photon_processes[proc].local_snd_info_ledger->entries[curr]);

  dbg_info("Spinning on info ledger looking for receive request");
  dbg_info("looking in position %d/%p", curr, curr_entry);

  count = 1;
  still_searching = 1;
  entry_iterator = curr_entry;
  do {
    while(entry_iterator->header == 0 || entry_iterator->footer == 0) {
      ;
    }
    if( (tag < 0) || (entry_iterator->tag == tag ) ) {
      still_searching = 0;
    }
    else {
      curr = (photon_processes[proc].local_snd_info_ledger->curr + count++) % photon_processes[proc].local_snd_info_ledger->num_entries;
      entry_iterator = &(photon_processes[proc].local_snd_info_ledger->entries[curr]);
    }
  }
  while(still_searching);
  
  /* If it wasn't the first pending receive request, swap the one we will serve (entry_iterator) with
     the first pending (curr_entry) in the info ledger, so that we can increment the current pointer
     (photon_processes[proc].local_snd_info_ledger->curr) and skip the request we will serve without losing any
     pending requests. */
  if( entry_iterator != curr_entry ) {
    tmp_entry = *entry_iterator;
    *entry_iterator = *curr_entry;
    *curr_entry = tmp_entry;
  }

  curr_entry->header = 0;
  curr_entry->footer = 0;

  if (request != NULL) {
    ret = __photon_setup_request_ledger(curr_entry, proc, request);
    if (ret != PHOTON_OK) {
      log_err("Could not setup request");
      goto error_exit;
    }
  }
  
  num_entries = photon_processes[proc].local_snd_info_ledger->num_entries;
  curr = photon_processes[proc].local_snd_info_ledger->curr;
  curr = (curr + 1) % num_entries;
  photon_processes[proc].local_snd_info_ledger->curr = curr;

  dbg_info("new curr == %d", photon_processes[proc].local_snd_info_ledger->curr);

  return PHOTON_OK;
error_exit:
  return PHOTON_ERROR;
}

static int _photon_wait_send_request_rdma(int tag) {
  photonRILedgerEntry curr_entry, entry_iterator;
  struct photon_ri_ledger_entry_t tmp_entry;
  int count, iproc;
#ifdef DEBUG
  time_t stime;
#endif
  int curr, num_entries, still_searching;

  dbg_info("(%d)", tag);

  dbg_info("Spinning on send info ledger looking for send request");

  still_searching = 1;
  iproc = -1;
#ifdef DEBUG
  stime = time(NULL);
#endif
  do {
    iproc = (iproc+1)%_photon_nproc;
    curr = photon_processes[iproc].local_snd_info_ledger->curr;
    curr_entry = &(photon_processes[iproc].local_snd_info_ledger->entries[curr]);
    dbg_info("looking in position %d/%p for proc %d", curr, curr_entry,iproc);

    count = 1;
    entry_iterator = curr_entry;
    // Some peers (procs) might have sent more than one send requests using different tags, so check them all.
    while(entry_iterator->header == 1 && entry_iterator->footer == 1) {
      if( (entry_iterator->addr == (uintptr_t)0) && (entry_iterator->priv.key0 == 0) && ((tag < 0) || (entry_iterator->tag == tag )) ) {
        still_searching = 0;
        dbg_info("Found matching send request with tag %d from proc %d", tag, iproc);
        break;
      }
      else {
        dbg_info("Found non-matching send request with tag %d from proc %d", tag, iproc);
        curr = (photon_processes[iproc].local_snd_info_ledger->curr + count) % photon_processes[iproc].local_snd_info_ledger->num_entries;
        ++count;
        entry_iterator = &(photon_processes[iproc].local_snd_info_ledger->entries[curr]);
      }
    }
#ifdef DEBUG
    stime = _tictoc(stime, -1);
#endif
  }
  while(still_searching);

  // If it wasn't the first pending send request, swap the one we will serve (entry_iterator) with
  // the first pending (curr_entry) in the send info ledger, so that we can increment the current pointer
  // (photon_processes[iproc].local_snd_info_ledger->curr) and skip the request we will serve without losing any
  // pending requests.
  if( entry_iterator != curr_entry ) {
    tmp_entry = *entry_iterator;
    *entry_iterator = *curr_entry;
    *curr_entry = tmp_entry;
  }

  curr_entry->header = 0;
  curr_entry->footer = 0;
  // NOTE:
  // curr_entry->request contains the curr_cookie_count om the sender size.	 In the current implementation we
  // are not doing anything with it.	Maybe we should keep it somehow and pass it back to the sender with
  // through post_recv_buffer().

  num_entries = photon_processes[iproc].local_snd_info_ledger->num_entries;
  curr = photon_processes[iproc].local_snd_info_ledger->curr;
  curr = (curr + 1) % num_entries;
  photon_processes[iproc].local_snd_info_ledger->curr = curr;

  dbg_info("new curr == %d", photon_processes[iproc].local_snd_info_ledger->curr);

  return PHOTON_OK;
}

static int _photon_post_os_put(uint32_t request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset) {
  photonRequest req;
  photonBI drb;
  photonBI db;
  uint64_t cookie;
  int rc;

  dbg_info("(%d, %p, %lu, %lu, %u)", proc, ptr, size, r_offset, request);

  if (htable_lookup(reqtable, (uint64_t)request, (void**)&req) != 0) {
    log_err("Could not find request");
    goto error_exit;
  }
  
  if (request != req->id) {
    log_err("Request mismatch encountered!");
    goto error_exit;
  }

  if (proc != req->proc) {
    log_err("Request/proc mismatch: %d/%d", proc, req->proc);
    goto error_exit;
  }
  
  /* photon_post_os_put() causes an RDMA transfer, but its own completion is
     communicated to the task that posts it through a completion event. */
  req->type = EVQUEUE;
  req->tag = tag;
  req->state = REQUEST_PENDING;

  /* get the remote buffer saved in the request */
  drb = &(req->remote_buffer);
  
  if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
    log_err("Tried posting a send for a buffer not registered");
    goto error_exit;
  }

  if (drb->buf.size > 0 && size + r_offset > drb->buf.size) {
    log_err("Requested to send %lu bytes to a %lu buffer size at offset %lu", size, drb->buf.size, r_offset);
    goto error_exit;
  }

  cookie = (( (uint64_t)proc)<<32) | request;
  dbg_info("Posted Cookie: %u/%u/%"PRIx64, proc, request, cookie);

  {
    rc = __photon_backend->rdma_put(proc, (uintptr_t)ptr, drb->buf.addr + (uintptr_t)r_offset,
                                    size, &(db->buf), &(drb->buf), cookie);

    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT failed for %lu\n", cookie);
      goto error_exit;
    }
  }

  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}

static int _photon_post_os_get(uint32_t request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset) {
  photonRequest req;
  photonBI drb;
  photonBI db;
  uint64_t cookie;
  int rc;

  dbg_info("(%d, %p, %lu, %lu, %u)", proc, ptr, size, r_offset, request);

  if (htable_lookup(reqtable, (uint64_t)request, (void**)&req) != 0) {
    log_err("Could not find request");
    goto error_exit;
  }
  
  if (request != req->id) {
    log_err("Request mismatch encountered!");
    goto error_exit;
  }

  if (proc != req->proc) {
    log_err("Request/proc mismatch: %d/%d", proc, req->proc);
    goto error_exit;
  }
  
  /* photon_post_os_get() causes an RDMA transfer, but its own completion is
     communicated to the task that posts it through a completion event. */
  req->type = EVQUEUE;
  req->tag = tag;
  req->state = REQUEST_PENDING;

  /* get the remote buffer saved in the request */
  drb = &(req->remote_buffer);

  if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
    log_err("Tried posting a os_get() into a buffer that's not registered");
    return -1;
  }

  if ( (drb->buf.size > 0) && ((size+r_offset) > drb->buf.size) ) {
    log_err("Requested to get %lu bytes from a %lu buffer size at offset %lu", size, drb->buf.size, r_offset);
    return -2;
  }

  cookie = (( (uint64_t)proc)<<32) | request;
  dbg_info("Posted Cookie: %u/%u/%"PRIx64, proc, request, cookie);

  {

    rc = __photon_backend->rdma_get(proc, (uintptr_t)ptr, drb->buf.addr + (uintptr_t)r_offset,
                                    size, &(db->buf), &(drb->buf), cookie);

    if (rc != PHOTON_OK) {
      dbg_err("RDMA GET failed for %lu\n", cookie);
      goto error_exit;
    }
  }

  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}

static int _photon_post_os_put_direct(int proc, void *ptr, uint64_t size, int tag, photonBuffer rbuf, uint32_t *request) {
  photonBI db;
  uint64_t cookie;
  int rc;
  uint32_t request_id;

  dbg_info("(%d, %p, %lu, %lu, %p)", proc, ptr, size, rbuf->size, request);

  if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
    log_err("Tried posting a os_put_direct() into a buffer that's not registered");
    return -1;
  }

  request_id = INC_COUNTER(curr_cookie);
  dbg_info("Incrementing curr_cookie_count to: %d", request_id);

  cookie = (( (uint64_t)proc)<<32) | request_id;
  dbg_info("Posted Cookie: %u/%u/%"PRIx64, proc, request_id, cookie);

  {

    rc = __photon_backend->rdma_put(proc, (uintptr_t)ptr, rbuf->addr,
                                    rbuf->size, &(db->buf), rbuf, cookie);
    
    if (rc != PHOTON_OK) {
      dbg_err("RDMA GET failed for %lu\n", cookie);
      goto error_exit;
    }
  }

  if (request != NULL) {
    *request = request_id;
    
    rc = __photon_setup_request_direct(rbuf, proc, tag, request_id);
    if (rc != PHOTON_OK) {
      dbg_info("Could not setup direct buffer request");
      goto error_exit;
    }
  }

  return PHOTON_OK;

error_exit:
  if (request != NULL) {
    *request = NULL_COOKIE;
  }
  return PHOTON_ERROR;
}

static int _photon_post_os_get_direct(int proc, void *ptr, uint64_t size, int tag, photonBuffer rbuf, uint32_t *request) {
  photonBI db;
  uint64_t cookie;
  int rc;
  uint32_t request_id;

  dbg_info("(%d, %p, %lu, %lu, %p)", proc, ptr, size, rbuf->size, request);

  if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
    log_err("Tried posting a os_get_direct() into a buffer that's not registered");
    return -1;
  }

  request_id = INC_COUNTER(curr_cookie);
  dbg_info("Incrementing curr_cookie_count to: %d", request_id);

  cookie = (( (uint64_t)proc)<<32) | request_id;
  dbg_info("Posted Cookie: %u/%u/%"PRIx64, proc, request_id, cookie);

  {

    rc = __photon_backend->rdma_get(proc, (uintptr_t)ptr, rbuf->addr, rbuf->size,
                                    &(db->buf), rbuf, cookie);

    if (rc != PHOTON_OK) {
      dbg_err("RDMA GET failed for %lu\n", cookie);
      goto error_exit;
    }
  }

  if (request != NULL) {
    *request = request_id;
    
    rc = __photon_setup_request_direct(rbuf, proc, tag, request_id);
    if (rc != PHOTON_OK) {
      dbg_info("Could not setup direct buffer request");
      goto error_exit;
    }
  }

  return PHOTON_OK;

error_exit:
  if (request != NULL) {
    *request = NULL_COOKIE;
  }
  return PHOTON_ERROR;
}

static int _photon_send_FIN(uint32_t request, int proc) {
  photonRequest req;
  photonFINLedgerEntry entry;
  int curr, num_entries, rc;
  uint64_t cookie;

  dbg_info("(%d)", proc);

  if (htable_lookup(reqtable, (uint64_t)request, (void**)&req) != 0) {
    dbg_info("Could not find request: %u", request);
    goto error_exit;
  }

  if (req->state != REQUEST_COMPLETED) {
    dbg_info("Warning: sending FIN for a request (EVQUEUE) that has not yet completed");
  }
  
  if (req->remote_buffer.request == NULL_COOKIE) {
    log_err("Trying to FIN a remote buffer request that was never set!");
    goto error_exit;
  }

  curr = photon_processes[proc].remote_FIN_ledger->curr;
  entry = &photon_processes[proc].remote_FIN_ledger->entries[curr];
  dbg_info("photon_processes[%d].remote_FIN_ledger->curr==%d",proc, curr);
  
  if( entry == NULL ) {
    log_err("entry is NULL for proc=%d",proc);
    goto error_exit;
  }
  
  entry->header = 1;
  entry->request = req->remote_buffer.request;
  entry->footer = 1;

  {
    uintptr_t rmt_addr;
    rmt_addr  = photon_processes[proc].remote_FIN_ledger->remote.addr;
    rmt_addr += photon_processes[proc].remote_FIN_ledger->curr * sizeof(*entry);
    cookie = (( (uint64_t)proc)<<32) | NULL_COOKIE;

    rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
                                    &(photon_processes[proc].remote_rcv_info_ledger->remote), cookie);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT failed for %lu\n", cookie);
      goto error_exit;
    }
  }

  num_entries = photon_processes[proc].remote_FIN_ledger->num_entries;
  curr = photon_processes[proc].remote_FIN_ledger->curr;
  curr = (curr + 1) % num_entries;
  photon_processes[proc].remote_FIN_ledger->curr = curr;

  if (req->state == REQUEST_COMPLETED) {
    dbg_info("Removing request %u for remote buffer request %u", request, req->remote_buffer.request);
    htable_remove(reqtable, (uint64_t)req->id, NULL);
    SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
    dbg_info("%d requests left in reqtable", htable_count(reqtable));
  }    

  req->flags = REQUEST_FLAG_FIN;
  req->remote_buffer.request = NULL_COOKIE;

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int _photon_wait_any(int *ret_proc, uint32_t *ret_req) {
  int rc;

  dbg_info("remaining: %d", htable_count(reqtable));

  if (ret_req == NULL) {
    goto error_exit;
  }

  if (htable_count(reqtable) == 0) {
    log_err("No events on queue to wait on");
    goto error_exit;
  }

  while(1) {
    uint32_t cookie;
    int existed;
    photon_event_status event;

    rc = __photon_backend->get_event(&event);
    if (rc != PHOTON_OK) {
      dbg_err("Could not get event");
      goto error_exit;
    }

    cookie = (uint32_t)( (event.id<<32)>>32);
    //fprintf(stderr,"gen_wait_any() poped an events with cookie:%x\n",cookie);
    if (cookie != NULL_COOKIE) {
      photonRequest req;
      void *test;

      dbg_info("removing event with cookie:%u", cookie);
      existed = htable_remove(ledger_reqtable, (uint64_t)cookie, &test);
      req = test;
      SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
    }
    else {
      existed = -1;
    }

    if (existed == -1) {
      continue;
    }
    else {
      *ret_req = cookie;
      *ret_proc = (uint32_t)(event.id>>32);
      return PHOTON_OK;
    }
  }

  return PHOTON_OK;
error_exit:
  return PHOTON_ERROR;
}

static int _photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req) {
  static int i = -1; // this is static so we don't starve events in later processes
  int curr, num_entries;

  dbg_info("remaining: %d", htable_count(reqtable));

  if (ret_req == NULL || ret_proc == NULL) {
    goto error_exit;
  }

  if (htable_count(ledger_reqtable) == 0) {
    log_err("No events on queue to wait_one()");
    goto error_exit;
  }

  while(1) {
    photonFINLedgerEntry curr_entry;
    int exists;

    i=(i+1)%_photon_nproc;

    // check if an event occurred on the RDMA end of things
    curr = photon_processes[i].local_FIN_ledger->curr;
    curr_entry = &(photon_processes[i].local_FIN_ledger->entries[curr]);

    if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
      void *test;
      dbg_info("Wait All In: %d/%u", photon_processes[i].local_FIN_ledger->curr, curr_entry->request);
      curr_entry->header = 0;
      curr_entry->footer = 0;

      exists = htable_remove(ledger_reqtable, (uint64_t)curr_entry->request, &test);
      if (exists != -1) {
        photonRequest req;
        req = test;
        *ret_req = curr_entry->request;
        *ret_proc = i;
        SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
        break;
      }

      num_entries = photon_processes[i].local_FIN_ledger->num_entries;
      curr = photon_processes[i].local_FIN_ledger->curr;
      curr = (curr + 1) % num_entries;
      photon_processes[i].local_FIN_ledger->curr = curr;
      dbg_info("Wait All Out: %d", curr);
    }
  }

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int _photon_probe_ledger(int proc, int *flag, int type, photonStatus status) {
  photonRILedger ledger;
  photonRILedgerEntry entry_iterator;
  int i, j;
  int start, end;

  dbg_info("(%d, %d)", proc, type);

  *flag = -1;

  if (proc == PHOTON_ANY_SOURCE) {
    start = 0;
    end = _photon_nproc;
  }
  else {
    start = proc;
    end = proc+1;
  }

  for (i=start; i<end; i++) {

    switch (type) {
    case PHOTON_SEND_LEDGER:
      ledger = photon_processes[i].local_snd_info_ledger;
      break;
    case PHOTON_RECV_LEDGER:
      ledger = photon_processes[i].local_rcv_info_ledger;
      break;
    default:
      dbg_err("unknown ledger type");
      goto error_exit;
    }

    for (j=0; j<ledger->num_entries; j++) {
      entry_iterator = &(ledger->entries[j]);
      if (entry_iterator->header && entry_iterator->footer && (entry_iterator->tag > 0)) {
        *flag = i;
        status->src_addr.global.proc_id = i;
        status->request = entry_iterator->request;
        status->tag = entry_iterator->tag;
        status->size = entry_iterator->size;

        dbg_info("Request: %u", entry_iterator->request);
        dbg_info("Address: %p", (void *)entry_iterator->addr);
        dbg_info("Size: %lu", entry_iterator->size);
        dbg_info("Tag: %d", entry_iterator->tag);

        *flag = 1;

        return PHOTON_OK;
      }
    }
  }

error_exit:
  return PHOTON_ERROR;
}

/* similar to photon_test()
   0 if some request ready to pop
   1 if no request found
  -1 on error */
static int _photon_probe(photonAddr addr, int *flag, photonStatus status) {
  //char buf[40];
  //inet_ntop(AF_INET6, addr->raw, buf, 40);
  //dbg_info("(%s)", buf);

  photonRequest req;
  int rc;
 
  req = SLIST_FIRST(&pending_recv_list);
  if (req) {
    SAFE_SLIST_REMOVE_HEAD(&pending_recv_list, slist);
    *flag = 1;
    status->src_addr.global.proc_id = req->proc;
    status->request = req->id;
    status->tag = req->tag;
    status->size = req->length;
    status->count = 1;
    status->error = 0;
    dbg_info("returning 0, flag:1");
    return PHOTON_OK;
  }
  else {
    *flag = 0;
    rc = __photon_nbpop_sr(NULL);
    dbg_info("returning %d, flag:0", rc);
    return rc;
  }
}

/* begin I/O */
static int _photon_io_init(char *file, int amode, MPI_Datatype view, int niter) {
  /* forwarders do our I/O for now */
  if (__photon_forwarder != NULL) {
    return __photon_forwarder->io_init(photon_processes, file, amode, view, niter);
  }
  else {
    return PHOTON_ERROR;
  }
}

static int _photon_io_finalize() {
  /* forwarders do our I/O for now */
  if (__photon_forwarder != NULL) {
    return __photon_forwarder->io_finalize(photon_processes);
  }
  else {
    return PHOTON_ERROR;
  }
}

/* TODO */
#ifdef PHOTON_MULTITHREADED
static inline int __photon_complete_ledger_req(uint32_t cookie) {
  photonRequest tmp_req;

  if (htable_lookup(ledger_reqtable, (uint64_t)cookie, (void**)&tmp_req) != 0)
    return -1;

  dbg_info("completing ledger req %"PRIx32, cookie);
  pthread_mutex_lock(&tmp_req->mtx);
  {
    tmp_req->state = REQUEST_COMPLETED;
    SAFE_LIST_INSERT_HEAD(&unreaped_ledger_reqs_list, tmp_req, list);
    pthread_cond_broadcast(&tmp_req->completed);
  }
  pthread_mutex_unlock(&tmp_req->mtx);

  return 0;
}

static inline int __photon_complete_evd_req(uint32_t cookie) {
  photonRequest tmp_req;

  if (htable_lookup(reqtable, (uint64_t)cookie, (void**)&tmp_req) != 0)
    return -1;

  dbg_info("completing event req %"PRIx32, cookie);
  pthread_mutex_lock(&tmp_req->mtx);
  {
    tmp_req->state = REQUEST_COMPLETED;
    SAFE_LIST_INSERT_HEAD(&unreaped_evd_reqs_list, tmp_req, list);
    pthread_cond_broadcast(&tmp_req->completed);
  }
  pthread_mutex_unlock(&tmp_req->mtx);

  return 0;
}

static void *__photon_req_watcher(void *arg) {
  int i, rc;
  int curr;
  uint32_t cookie;
  photon_event_status event;

  dbg_info("reqs watcher started");

  while(1) {
    // First we poll for CQEs and clear reqs waiting on them.
    // We don't want to spend too much time on this before moving to ledgers.

    //should get more events per call here
    rc = __photon_backend->get_event(&event);
    if (rc != PHOTON_OK) {
      dbg_err("Could not get event");
      goto error_exit;
    }

    cookie = (uint32_t)( (event.id<<32)>>32);

    if (cookie == NULL_COOKIE)
      continue;

    if (!__photon_complete_ledger_req(cookie))
      continue;

    if (!__photon_complete_evd_req(cookie))
      continue;

    // TODO: Is this the only other possibility?
    if( DEC_COUNTER(handshake_rdma_write) <= 0 )
      log_err("handshake_rdma_write_count is negative");

    for(i = 0; i < _photon_nproc; i++) {
      photon_rdma_FIN_ledger_entry_t *curr_entry;
      curr = photon_processes[i].local_FIN_ledger->curr;
      curr_entry = &(photon_processes[i].local_FIN_ledger->entries[curr]);
      if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
        dbg_info("found: %d/%u", curr, curr_entry->request);
        curr_entry->header = 0;
        curr_entry->footer = 0;

        if (__photon_complete_ledger_req(curr_entry->request))
          log_err("couldn't find req for FIN ledger: %u", curr_entry->request);

        photon_processes[i].local_FIN_ledger->curr = (photon_processes[i].local_FIN_ledger->curr + 1) % photon_processes[i].local_FIN_ledger->num_entries;
        dbg_info("%d requests left in reqtable", htable_count(ledger_reqtable));
      }
    }
  }

  pthread_exit(NULL);
}

#endif

/* begin util */
int _photon_handle_addr(photonAddr addr, photonAddr raddr) {
  if (!raddr) {
    return PHOTON_ERROR;
  }

  // see if we have a block_id to send to
  if (!(addr->blkaddr.blk0) &&
      !(addr->blkaddr.blk1) &&
      !(addr->blkaddr.blk2) && addr->blkaddr.blk3) {
    if (__photon_config->ud_gid_prefix) {
      inet_pton(AF_INET6, __photon_config->ud_gid_prefix, raddr->raw);
      uint32_t *iptr = (uint32_t*)&(raddr->raw[12]);
      *iptr = htonl(addr->blkaddr.blk3);
    }
    else {
      dbg_err("block_id, missing ud_gid_prefix?");
      return PHOTON_ERROR;
    }
  }
  else {
    raddr->global.prefix = addr->global.prefix;
    raddr->global.proc_id = addr->global.proc_id;
  }

  return PHOTON_OK;
}

int _photon_get_buffer_private(void *buf, uint64_t size, photonBufferPriv ret_priv) {
  photonBI db;

  if (buffertable_find_exact(buf, size, &db) == 0) {
    return photon_buffer_get_private(db, ret_priv);
  }
  else {
    return PHOTON_ERROR;
  }
}

int _photon_get_buffer_remote(uint32_t request, photonBuffer ret_buf) {
  photonRequest req;

  if (htable_lookup(reqtable, (uint64_t)request, (void**)&req) != 0) {
    dbg_info("Could not find request: %u", request);
    goto error_exit;
  }

  if ((req->state != REQUEST_NEW) && (req->state != REQUEST_PENDING)) {
    dbg_info("Request has already trasitioned, can not return remote buffer info");
    goto error_exit;
  }
  
  if (ret_buf) {
    (*ret_buf).addr = req->remote_buffer.buf.addr;
    (*ret_buf).size = req->remote_buffer.buf.size;
    (*ret_buf).priv = req->remote_buffer.buf.priv;
  }

  return PHOTON_OK;

 error_exit:
  ret_buf = NULL;
  return PHOTON_ERROR;
}
/* end util */

#ifdef HAVE_XSP
int photon_xsp_lookup_proc(libxspSess *sess, ProcessInfo **ret_pi, int *index) {
  int i;

  for (i = 0; i < _photon_nproc; i++) {
    if (photon_processes[i].sess &&
        !xsp_sesscmp(photon_processes[i].sess, sess)) {
      if (index)
        *index = i;
      *ret_pi = &photon_processes[i];
      return PHOTON_OK;
    }
  }

  if (index)
    *index = -1;
  *ret_pi = NULL;
  return PHOTON_ERROR;
}

int photon_xsp_unused_proc(ProcessInfo **ret_pi, int *index) {
  int i;

  /* find a process struct that has no session... */
  for (i = 0; i < _photon_nproc; i++) {
    if (!photon_processes[i].sess)
      break;
  }

  if (i == _photon_nproc) {
    if (index)
      *index = -1;
    *ret_pi = NULL;
    return PHOTON_ERROR;
  }

  if (index)
    *index = i;

  *ret_pi = &photon_processes[i];
  return PHOTON_OK;
}
#endif
