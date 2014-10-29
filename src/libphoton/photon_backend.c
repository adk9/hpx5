#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "libphoton.h"
#include "photon_backend.h"
#include "photon_forwarder.h"
#include "photon_buffertable.h"
#include "photon_exchange.h"
#include "photon_pwc.h"
#include "photon_request.h"
#include "photon_event.h"

photonBI             shared_storage;
photonRequestTable   request_table;
ProcessInfo         *photon_processes;
htable_t            *reqtable, *pwc_reqtable, *sr_reqtable;
photonMsgBuf         sendbuf;
photonMsgBuf         recvbuf;

static photonRequest requests;
static int           num_requests;

/* default backend methods */
static int _photon_initialized(void);
static int _photon_init(photonConfig cfg, ProcessInfo *info, photonBI ss);
static int _photon_cancel(photon_rid request, int flags);
static int _photon_finalize(void);
static int _photon_register_buffer(void *buffer, uint64_t size);
static int _photon_unregister_buffer(void *buffer, uint64_t size);
static int _photon_test(photon_rid request, int *flag, int *type, photonStatus status);
static int _photon_wait(photon_rid request);
static int _photon_send(photonAddr addr, void *ptr, uint64_t size, int flags, photon_rid *request);
static int _photon_recv(uint64_t request, void *ptr, uint64_t size, int flags);
static int _photon_post_recv_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, photon_rid *request);
static int _photon_post_send_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, photon_rid *request);
static int _photon_post_send_request_rdma(int proc, uint64_t size, int tag, photon_rid *request);
static int _photon_wait_recv_buffer_rdma(int proc, uint64_t size, int tag, photon_rid *request);
static int _photon_wait_send_buffer_rdma(int proc, uint64_t size, int tag, photon_rid *request);
static int _photon_wait_send_request_rdma(int tag);
static int _photon_post_os_put(photon_rid request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset);
static int _photon_post_os_get(photon_rid request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset);
static int _photon_post_os_put_direct(int proc, void *ptr, uint64_t size, photonBuffer rbuf, int flags, photon_rid *request);
static int _photon_post_os_get_direct(int proc, void *ptr, uint64_t size, photonBuffer rbuf, int flags, photon_rid *request);
static int _photon_send_FIN(photon_rid request, int proc, int flags);
static int _photon_wait_any(int *ret_proc, photon_rid *ret_req);
static int _photon_wait_any_ledger(int *ret_proc, photon_rid *ret_req);
static int _photon_probe_ledger(int proc, int *flag, int type, photonStatus status);
static int _photon_probe(photonAddr addr, int *flag, photonStatus status);
static int _photon_io_init(char *file, int amode, void *view, int niter);
static int _photon_io_finalize();

/*
   We only want to spawn a dedicated thread for ledgers on
   multithreaded instantiations of the library (e.g. in xspd).
*/

#ifdef PHOTON_MULTITHREADED
static pthread_t event_watcher;
static void *__photon_event_watcher(void *arg);
static pthread_t ledger_watcher;
static void *__photon_req_watcher(void *arg);
#endif

struct photon_backend_t photon_default_backend = {
  .initialized = _photon_initialized,
  .init = _photon_init,
  .cancel = _photon_cancel,
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
  .put_with_completion = _photon_put_with_completion,
  .get_with_completion = _photon_get_with_completion,
  .probe_completion = _photon_probe_completion,
  .rdma_get = NULL,
  .rdma_put = NULL,
  .rdma_send = NULL,
  .rdma_recv = NULL,
  .get_event = NULL
};

static int _photon_initialized() {
  if (__photon_backend && __photon_config)
    return __photon_backend->initialized();
  else
    return PHOTON_ERROR_NOINIT;
}

static int _photon_init(photonConfig cfg, ProcessInfo *info, photonBI ss) {
  int i, rc;
  char *buf;
  int bufsize;
  int info_ledger_size, fin_ledger_size, pwc_ledger_size, eager_ledger_size;
  int eager_bufsize, pwc_bufsize;

  srand48(getpid() * time(NULL));

  dbg_trace("(nproc %d, rank %d)",_photon_nproc, _photon_myrank);

  INIT_COUNTER(curr_cookie, 1);

  requests = malloc(sizeof(struct photon_req_t) * DEF_NUM_REQUESTS);
  if (!requests) {
    log_err("Failed to allocate request list");
    goto error_exit_req;
  }

  num_requests = DEF_NUM_REQUESTS;
  LIST_INIT(&free_reqs_list);
  SLIST_INIT(&pending_pwc_list);
  SLIST_INIT(&pending_recv_list);

  for(i = 0; i < num_requests; i++) {
    requests[i].mmask = bit_array_create(UD_MASK_SIZE);
    LIST_INSERT_HEAD(&free_reqs_list, &(requests[i]), list);
  }

  dbg_trace("num ledgers: %d", _LEDGER_SIZE);
  dbg_trace("eager buf size: %d", _photon_ebsize);
  dbg_trace("small msg size: %d", _photon_smsize);

  dbg_trace("create_buffertable()");
  fflush(stderr);
  if (buffertable_init(193)) {
    log_err("Failed to allocate buffer table");
    goto error_exit_req;
  }

  dbg_trace("create_reqtable()");
  reqtable = htable_create(193);
  if (!reqtable) {
    log_err("Failed to allocate request table");
    goto error_exit_bt;
  }

  dbg_trace("create_pwc_reqtable()");

  pwc_reqtable = htable_create(193);
  if (!pwc_reqtable) {
    log_err("Failed to allocate PWC request table");
    goto error_exit_rt;
  }

  dbg_trace("create_sr_reqtable()");

  sr_reqtable = htable_create(193);
  if (!sr_reqtable) {
    log_err("Failed to allocate SR request table");
    goto error_exit_pwct;
  }
  
  photon_processes = (ProcessInfo *) malloc(sizeof(ProcessInfo) * (_photon_nproc + _photon_nforw));
  if (!photon_processes) {
    log_err("Couldn't allocate process information");
    goto error_exit_srt;
  }

  // Set it to zero, so that we know if it ever got initialized
  memset(photon_processes, 0, sizeof(ProcessInfo) * (_photon_nproc + _photon_nforw));

  dbg_trace("alloc'd process info");

  // Ledgers are x2 cause we need a local and a remote copy of each ledger.
  // Info ledger has an additional x2 cause we need "send-info" and "receive-info" ledgers.
  info_ledger_size = 2 * 2 * PHOTON_NP_INFO_SIZE;
  fin_ledger_size  = 2 * PHOTON_NP_LEDG_SIZE;
  pwc_ledger_size = 2 * PHOTON_NP_LEDG_SIZE;
  eager_ledger_size = 2 * PHOTON_NP_LEDG_SIZE;
  eager_bufsize = 2 * PHOTON_NP_EBUF_SIZE;
  pwc_bufsize = 2 * PHOTON_NP_PBUF_SIZE;
  bufsize = info_ledger_size + fin_ledger_size + pwc_ledger_size + eager_ledger_size;
  bufsize += (eager_bufsize + pwc_bufsize);

  if (!eager_bufsize) {
    log_warn("EAGER buffers disabled!");
  }

  rc = posix_memalign((void**)&buf, getpagesize(), bufsize);
  if (rc || !buf) {
    log_err("Couldn't allocate ledgers");
    goto error_exit_crb;
  }
  dbg_trace("Bufsize: %d", bufsize);

  if (photon_setup_ri_ledger(photon_processes, PHOTON_LRI_PTR(buf), _LEDGER_SIZE) != 0) {
    log_err("couldn't setup snd/rcv info ledgers");
    goto error_exit_buf;
  }

  if (photon_setup_fin_ledger(photon_processes, PHOTON_LF_PTR(buf), _LEDGER_SIZE) != 0) {
    log_err("couldn't setup send ledgers");
    goto error_exit_buf;
  }

  if (photon_setup_pwc_ledger(photon_processes, PHOTON_LP_PTR(buf), _LEDGER_SIZE) != 0) {
    log_err("couldn't setup send ledgers");
    goto error_exit_buf;
  }

  if (photon_setup_eager_ledger(photon_processes, PHOTON_LE_PTR(buf), _LEDGER_SIZE) != 0) {
    log_err("couldn't setup eager ledgers");
    goto error_exit_buf;
  }

  if (photon_setup_eager_buf(photon_processes, PHOTON_LEB_PTR(buf), _photon_ebsize) != 0) {
    log_err("couldn't setup eager buffers");
    goto error_exit_buf;
  }

  if (photon_setup_pwc_buf(photon_processes, PHOTON_LPB_PTR(buf), _photon_ebsize) != 0) {
    log_err("couldn't setup pwc eager buffers");
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

  // allocate buffers for UD send/recv operations (after backend initializes)
  uint64_t msgbuf_size, p_size;
  int p_offset, p_hsize;
  if (!strcmp(cfg->backend, "verbs") && cfg->ibv.use_ud) {
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
    
    //else {
    //  p_offset = 0;
    //  p_hsize = 0;
    //  p_size = p_offset + _photon_smsize;
    
    dbg_trace("sr partition size: %lu", p_size);
    
    // create enough space to accomodate every rank sending _LEDGER_SIZE messages
    msgbuf_size = _LEDGER_SIZE * p_size * _photon_nproc;
    
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
    for (i = 0; i < recvbuf->p_count; i++) {
      __photon_backend->rdma_recv(NULL, (uintptr_t)recvbuf->entries[i].base, recvbuf->p_size,
                                  &recvbuf->db->buf, (( (uint64_t)REQUEST_COOK_RECV) << 32) | i, 0);
    }
  }
  
  // register any buffers that were requested before init
  while( !SLIST_EMPTY(&pending_mem_register_list) ) {
    struct photon_mem_register_req *mem_reg_req;
    dbg_trace("registering buffer in queue");
    mem_reg_req = SLIST_FIRST(&pending_mem_register_list);
    SLIST_REMOVE_HEAD(&pending_mem_register_list, list);
    photon_register_buffer(mem_reg_req->buffer, mem_reg_req->buffer_size);
  }

#ifdef PHOTON_MULTITHREADED
  if (pthread_create(&event_watcher, NULL, __photon_event_watcher, NULL)) {
    log_err("pthread_create() failed");
    goto error_exit_sb;
  }
  if (pthread_create(&ledger_watcher, NULL, __photon_req_watcher, NULL)) {
    log_err("pthread_create() failed");
    goto error_exit_rb;
  }
#endif

  dbg_trace("ended successfully =============");

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
 error_exit_pwct:
  htable_free(pwc_reqtable);
 error_exit_rt:
  htable_free(reqtable);
 error_exit_bt:
  buffertable_finalize();
 error_exit_req:
  free(requests);
  DESTROY_COUNTER(curr_cookie);
  
  return PHOTON_ERROR;
}

static int _photon_cancel(photon_rid request, int flags) {
  photonRequest req;
  
  if (flags & PHOTON_SHUTDOWN) {
    
  }
  else {
    if (htable_lookup(reqtable, request, (void**)&req)) {
      dbg_err("Could not find request to cancel: 0x%016lx", request);
      return PHOTON_ERROR;
    }
    while (!(req->flags & REQUEST_FLAG_LDONE))
      __photon_nbpop_event(req);

    htable_remove(reqtable, request, NULL);
  }

  return PHOTON_OK;
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

  dbg_trace("(%p, %lu)", buffer, size);

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
    dbg_trace("called before init, queueing buffer info");
    goto normal_exit;
  }

  if (buffertable_find_exact(buffer, size, &db) == 0) {
    dbg_trace("we had an existing buffer, reusing it");
    db->ref_count++;
    goto normal_exit;
  }

  db = photon_buffer_create(buffer, size);
  if (!db) {
    log_err("could not create photon buffer");
    goto error_exit;
  }

  dbg_trace("created buffer: %p", db);

  if (photon_buffer_register(db, __photon_backend->context) != 0) {
    log_err("Couldn't register buffer");
    goto error_exit_db;
  }

  dbg_trace("registered buffer");

  if (buffertable_insert(db) != 0) {
    goto error_exit_db;
  }

  dbg_trace("added buffer to hash table");

normal_exit:
  return PHOTON_OK;
error_exit_db:
  photon_buffer_free(db);
error_exit:
  return PHOTON_ERROR;
}

static int _photon_unregister_buffer(void *buffer, uint64_t size) {
  photonBI db;

  dbg_trace("%llu", size);

  if(__photon_backend->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  if (buffertable_find_exact(buffer, size, &db) != 0) {
    dbg_trace("no such buffer is registered");
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
static int _photon_test(photon_rid request, int *flag, int *type, photonStatus status) {
  photonRequest req;
  void *test;
  int ret_val;

  dbg_trace("(0x%016lx)", request);

  if (htable_lookup(reqtable, request, &test) != 0) {
    if (htable_lookup(sr_reqtable, request, &test) != 0) {
      dbg_trace("Request is not in any request-table");
      // Unlike photon_wait(), we might call photon_test() multiple times on a request,
      // e.g., in an unguarded loop.	flag==-1 will signify that the operation is
      // not pending.	 This means, it might be completed, it might have never been
      // issued.	It's up to the application to guarantee correctness, by keeping
      // track, of	what's going on.	Unless you know what you are doing, consider
      // (flag==-1 && return_value==1) to be an error case.
      dbg_trace("returning 1, flag:-1");
      *flag = -1;
      return 1;
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
    status->size = req->length;
    status->count = 1;
    status->error = 0;
    dbg_trace("returning 0, flag:1");
    return 0;
  }
  else if( ret_val > 0 ) {
    dbg_trace("returning 0, flag:0");
    *flag = 0;
    return 0;
  }
  else {
    dbg_trace("returning -1, flag:0");
    *flag = 0;
    return -1;
  }
}

static int _photon_wait(photon_rid request) {
  photonRequest req;

  dbg_trace("(0x%016lx)",request);

  if (htable_lookup(reqtable, request, (void**)&req) != 0) {
    if (htable_lookup(sr_reqtable, request, (void**)&req) != 0) {
      log_err("Wrong request value, operation not in table");
      return -1;
    }
  }

#ifdef PHOTON_MULTITHREADED

  pthread_mutex_lock(&req->mtx);
  {
    while(req->state == REQUEST_PENDING)
      pthread_cond_wait(&req->completed, &req->mtx);

    if (req->type == SENDRECV) {
      if (htable_lookup(sr_reqtable, req->id, NULL) != -1) {
        dbg_trace("removing SR RDMA: %u", req->id);
        htable_remove(sr_reqtable, req->id, NULL);
        SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
        dbg_trace("%d requests left in sr_table", htable_count(sr_reqtable));
      }
    }
    else {
      if (htable_lookup(reqtable, req->id, NULL) != -1) {
        dbg_trace("removing event with cookie:%u", req->id);
        htable_remove(reqtable, req->id, NULL);
        SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
        dbg_trace("%d requests left in reqtable", htable_count(reqtable));
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

static int _photon_send(photonAddr addr, void *ptr, uint64_t size, int flags, photon_rid *request) {
#ifdef DEBUG
  char buf[40], buf2[40];
  inet_ntop(AF_INET6, addr->raw, buf, 40);
  dbg_trace("(%s, %p, %lu, %d)", buf, ptr, size, flags);
#endif

  photon_addr saddr;
  int bufs[DEF_MAX_BUF_ENTRIES];
  photon_rid request_id;
  uint64_t cookie;
  uint64_t bytes_remaining, bytes_sent, send_bytes;
  uintptr_t buf_addr;
  int rc, m_count, num_msgs;

  rc = _photon_handle_addr(addr, &saddr);
  if (rc != PHOTON_OK) {
    goto error_exit;
  }

#ifdef DEBUG
  inet_ntop(AF_INET6, saddr.raw, buf2, 40);
  dbg_trace("(%s, %p, %lu, %d)", buf2, ptr, size, flags);
#endif

  request_id = INC_COUNTER(curr_cookie);
  
  // segment and send as entries of the sendbuf
  bytes_remaining = size;
  bytes_sent = 0;
  m_count = 0;
  
  num_msgs = size / sendbuf->m_size + 1;
  if (num_msgs > DEF_MAX_BUF_ENTRIES) {
    dbg_err("Message of size %lu requires too many mbuf entries, %u, max=%u", size, num_msgs, DEF_MAX_BUF_ENTRIES);
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
    memcpy(bentry->mptr, (char*)ptr + bytes_sent, send_bytes);

    if (__photon_config->ibv.use_ud) {
      // create the header
      photon_ud_hdr *hdr = (photon_ud_hdr*)bentry->hptr;
      hdr->request = request_id;
      hdr->src_addr = (uint32_t)_photon_myrank;
      hdr->length = send_bytes;
      hdr->msn = m_count;
      hdr->nmsg = num_msgs;
    }

    dbg_trace("sending mbuf [%d/%d], size=%lu, header size=%d", m_count, num_msgs-1, send_bytes, sendbuf->p_hsize);

    buf_addr = (uintptr_t)bentry->hptr;
    rc = __photon_backend->rdma_send(&saddr, buf_addr, send_bytes + sendbuf->p_hsize, &sendbuf->db->buf, cookie, 0);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA SEND failed for 0x%016lx\n", cookie);
      goto error_exit;
    }
    
    bufs[m_count++] = b_ind;
    bytes_sent += send_bytes;
    bytes_remaining -= send_bytes;
  } while (bytes_remaining);
  
  if (request != NULL) {
    *request = request_id;
    
    rc = __photon_setup_request_send(addr, bufs, m_count, request_id);
    if (rc != PHOTON_OK) {
      dbg_trace("Could not setup sendrecv request");
      goto error_exit;
    }
  }  
  
  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

static int _photon_recv(photon_rid request, void *ptr, uint64_t size, int flags) {
  photonRequest req;

  dbg_trace("(0x%016lx, %p, %lu, %d)", request, ptr, size, flags);

  //FIXME: assume recv called right after probe to get the same req
  req = SLIST_FIRST(&pending_recv_list);

  if (req) {
    SAFE_SLIST_REMOVE_HEAD(&pending_recv_list, slist);

    if (req->length != size) {
      dbg_err("popped message does not match requested size");
      goto error_exit;
    }

    /*
    if (htable_lookup(sr_reqtable, request, (void**)&req) == 0) {
      if (request != req->id) {
      dbg_err("request id mismatch!");
      goto error_exit;
    }
    */

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
      memcpy((char*)ptr + bytes_copied, recvbuf->entries[bind].mptr, copy_bytes);

      // re-arm this buffer entry for another recv
      rc = __photon_backend->rdma_recv(NULL, (uintptr_t)recvbuf->entries[bind].base, recvbuf->p_size,
                                       &recvbuf->db->buf, (( (uint64_t)REQUEST_COOK_RECV) << 32) | bind, 0);
      if (rc != PHOTON_OK) {
        dbg_err("could not post_recv() buffer entry");
        goto error_exit;
      }

      m_count++;
      bytes_copied += copy_bytes;
      bytes_remaining -= copy_bytes;
    }
    
    //dbg_trace("recv request completed: 0x%016lx", req->id);
    //req->state == REQUEST_COMPLETED;
    dbg_trace("removing recv request from sr_reqtable: 0x%016lx", req->id);
    htable_remove(sr_reqtable, req->id, NULL);
    SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
  }
  else {
    dbg_trace("request not found in sr_reqtable");
    goto error_exit;
  }

  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

static int _photon_post_recv_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, photon_rid *request) {
  photonBI db;
  photonRILedgerEntry entry;
  int curr, rc;
  photon_rid request_id;

  dbg_trace("(%d, %p, %lu, %d, %p)", proc, ptr, size, tag, request);
  
  if (buffertable_find_containing( (void *) ptr, (int)size, &db) != 0) {
    log_err("Requested recv from ptr not in table");
    goto error_exit;
  }

  request_id = (( (uint64_t)proc)<<32) | INC_COUNTER(curr_cookie);
  dbg_trace("Incrementing curr_cookie_count to: 0x%016lx", request_id);

  /* proc == -1 means ANY_SOURCE.  In this case all potential senders must post a send request
     which will write into our snd_info ledger entries such that:
     rkey == 0
     addr == (uintptr_t)0  */
  if( proc == PHOTON_ANY_SOURCE ) {
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

  dbg_trace("Post recv");
  dbg_trace("Request: 0x%016lx", entry->request);
  dbg_trace("Address: %p", (void *)entry->addr);
  dbg_trace("Size: %lu", entry->size);
  dbg_trace("Tag: %d", entry->tag);
  dbg_trace("Keys: 0x%016lx / 0x%016lx", entry->priv.key0, entry->priv.key1);
  
  {
    uintptr_t rmt_addr;
    rmt_addr  = photon_processes[proc].remote_rcv_info_ledger->remote.addr;
    rmt_addr += photon_processes[proc].remote_rcv_info_ledger->curr * sizeof(*entry);

    rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
                                    &(photon_processes[proc].remote_rcv_info_ledger->remote), request_id, 0);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT failed for 0x%016lx", request_id);
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
    // this handshake is completed.  This will be reflected in the LEDGER by the corresponding
    // photon_send_FIN() posted by the sender.
    req->type = LEDGER;
    req->proc = proc;
    req->tag = tag;
    req->length = size;

    dbg_trace("Inserting the RDMA request into the request table: 0x%016lx/%p", request_id, req);
    if (htable_insert(reqtable, (uint64_t)request_id, req) != 0) {
      // this is bad, we've submitted the request, but we can't track it
      log_err("Couldn't save request in hashtable");
    }
    *request = request_id;
  }
  
  NEXT_LEDGER_ENTRY(photon_processes[proc].remote_rcv_info_ledger);
  dbg_trace("New curr (proc=%d): %u", proc, photon_processes[proc].remote_rcv_info_ledger->curr);

  return PHOTON_OK;

error_exit:
  if (request != NULL) {
    *request = NULL_COOKIE;
  }
  return PHOTON_ERROR;
}

static int _photon_post_send_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, photon_rid *request) {
  photonBI db;
  int curr, rc;
  photon_rid request_id;
  bool eager = false;

  dbg_trace("(%d, %p, %lu, %d, %p)", proc, ptr, size, tag, request);
  
  if (buffertable_find_containing( (void*)ptr, (int)size, &db) != 0) {
    log_err("Requested post of send buffer for ptr not in table");
    goto error_exit;
  }

  request_id = (( (uint64_t)proc)<<32) | INC_COUNTER(curr_cookie);
  dbg_trace("Incrementing curr_cookie_count to: 0x%016lx", request_id);
  
  {
    if (size <= _photon_smsize) {
      uintptr_t rmt_addr, eager_addr;
      uint64_t eager_cookie;
      photonLedgerEntry entry;
      photonEagerBuf eb;

      curr = photon_processes[proc].remote_eager_ledger->curr;

      //eager_addr = (uintptr_t)photon_processes[proc].remote_eager_buf->remote.addr + 
      //(sizeof(struct photon_rdma_eager_buf_entry_t) * curr);

      eb = photon_processes[proc].remote_eager_buf;
      if ((eb->offset + size) > _photon_ebsize)
        NEXT_EAGER_BUF(eb, size);

      eager_addr = (uintptr_t)eb->remote.addr + eb->offset;
      eager_cookie = (( (uint64_t)REQUEST_COOK_EAGER)<<32) | request_id;
      
      dbg_trace("EAGER PUT of size %lu to addr: 0x%016lx", size, eager_addr);
      
      rc = __photon_backend->rdma_put(proc, (uintptr_t)ptr, eager_addr, size, &(db->buf),
				      &eb->remote, eager_cookie, 0);

      if (rc != PHOTON_OK) {
	dbg_err("RDMA EAGER PUT failed for 0x%016lx", eager_cookie);
	goto error_exit;
      }
      NEXT_EAGER_BUF(eb, size);

      rmt_addr  = photon_processes[proc].remote_eager_ledger->remote.addr;
      rmt_addr += photon_processes[proc].remote_eager_ledger->curr * sizeof(*entry);

      entry = &photon_processes[proc].remote_eager_ledger->entries[curr]; 
      // encode the eager size and request id in the eager ledger
      entry->request = (size<<32) | (request_id<<32>>32);

      dbg_trace("Updating remote eager ledger address: 0x%016lx, %lu", rmt_addr, sizeof(*entry));
     
      rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
                                      &(photon_processes[proc].remote_eager_ledger->remote), request_id, 0);
      if (rc != PHOTON_OK) {
        dbg_err("RDMA PUT failed for 0x%016lx", request_id);
        goto error_exit;
      }
      eager = true;

      NEXT_LEDGER_ENTRY(photon_processes[proc].remote_eager_ledger);
      dbg_trace("new eager curr == %d", photon_processes[proc].remote_eager_ledger->curr);
    }
    else {
      uintptr_t rmt_addr;
      photonRILedgerEntry entry;

      rmt_addr  = photon_processes[proc].remote_snd_info_ledger->remote.addr;
      rmt_addr += photon_processes[proc].remote_snd_info_ledger->curr * sizeof(*entry);
      
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
      entry->flags = REQUEST_FLAG_NIL;
      
      dbg_trace("Post send request");
      dbg_trace("Request: 0x%016lx", entry->request);
      dbg_trace("Addr: %p", (void *)entry->addr);
      dbg_trace("Size: %lu", entry->size);
      dbg_trace("Tag: %d", entry->tag);
      dbg_trace("Keys: 0x%016lx / 0x%016lx", entry->priv.key0, entry->priv.key1);
      dbg_trace("Updating remote ledger address: 0x%016lx, %lu", rmt_addr, sizeof(*entry));

      rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
                                      &(photon_processes[proc].remote_snd_info_ledger->remote), request_id, 0);
      if (rc != PHOTON_OK) {
        dbg_err("RDMA PUT failed for 0x%016lx", request_id);
        goto error_exit;
      }

      NEXT_LEDGER_ENTRY(photon_processes[proc].remote_snd_info_ledger);
      dbg_trace("new curr == %d", photon_processes[proc].remote_snd_info_ledger->curr);
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
    req->proc = proc;
    req->tag = tag;
    req->length = size;
    req->flags = (eager)?REQUEST_FLAG_EAGER:REQUEST_FLAG_NIL;

    req->type = LEDGER;
    dbg_trace("Inserting the RDMA request into the request table: 0x%016lx/%p", request_id, req);
    if (htable_insert(reqtable, (uint64_t)request_id, req) != 0) {
      log_err("Couldn't save request in hashtable");
    }

    *request = request_id;
  }

  return PHOTON_OK;

error_exit:
  if (request != NULL) {
    *request = NULL_COOKIE;
  }
  return PHOTON_ERROR;
}

static int _photon_post_send_request_rdma(int proc, uint64_t size, int tag, photon_rid *request) {
  photonRILedgerEntry entry;
  int curr, rc;
  photon_rid request_id;

  dbg_trace("(%d, %lu, %d, %p)", proc, size, tag, request);

  request_id = (( (uint64_t)proc)<<32) | INC_COUNTER(curr_cookie);
  dbg_trace("Incrementing curr_cookie_count to: 0x%016lx", request_id);

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

  dbg_trace("Post send request");
  dbg_trace("Request: 0x%016lx", entry->request);
  dbg_trace("Addr: %p", (void *)entry->addr);
  dbg_trace("Size: %lu", entry->size);
  dbg_trace("Tag: %d", entry->tag);
  dbg_trace("Keys: 0x%016lx / 0x%016lx", entry->priv.key0, entry->priv.key1);

  {
    uintptr_t rmt_addr;
    rmt_addr  = photon_processes[proc].remote_snd_info_ledger->remote.addr;
    rmt_addr += photon_processes[proc].remote_snd_info_ledger->curr * sizeof(*entry);

    rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
                                    &(photon_processes[proc].remote_rcv_info_ledger->remote), request_id, 0);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT failed for 0x%016lx", request_id);
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

    dbg_trace("Inserting the RDMA request into the request table: %lu/%p", request_id, req);
    if (htable_insert(reqtable, (uint64_t)request_id, req) != 0) {
      // this is bad, we've submitted the request, but we can't track it
      log_err("Couldn't save request in hashtable");
    }
    *request = request_id;
  }

  NEXT_LEDGER_ENTRY(photon_processes[proc].remote_snd_info_ledger);

  dbg_trace("new curr == %d", photon_processes[proc].remote_snd_info_ledger->curr);

  return PHOTON_OK;

error_exit:
  if (request != NULL) {
    *request = NULL_COOKIE;
  }
  return PHOTON_ERROR;
}

static int _photon_wait_recv_buffer_rdma(int proc, uint64_t size, int tag, photon_rid *request) {
  photonRILedgerEntry curr_entry, entry_iterator;
  struct photon_ri_ledger_entry_t tmp_entry;
  int ret, count, curr, still_searching, num_entries;

  dbg_trace("(%d, %d)", proc, tag);
  dbg_trace("Spinning on info ledger looking for receive request");
  dbg_trace("curr == %d", photon_processes[proc].local_rcv_info_ledger->curr);

  curr = photon_processes[proc].local_rcv_info_ledger->curr;
  curr_entry = &(photon_processes[proc].local_rcv_info_ledger->entries[curr]);

  dbg_trace("looking in position %d/%p", photon_processes[proc].local_rcv_info_ledger->curr, curr_entry);

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

  if (request != NULL) {
    ret = __photon_setup_request_ledger_info(curr_entry, curr, proc, request);
    if (ret != PHOTON_OK) {
      log_err("Could not setup request");
      goto error_exit;
    }
  }

  NEXT_LEDGER_ENTRY(photon_processes[proc].local_rcv_info_ledger);

  dbg_trace("new curr == %d", photon_processes[proc].local_rcv_info_ledger->curr);
  
  return PHOTON_OK;
 error_exit:
  return PHOTON_ERROR;
}

static int _photon_wait_send_buffer_rdma(int proc, uint64_t size, int tag, photon_rid *request) {
  photonLedgerEntry eager_entry;
  photonRILedgerEntry curr_entry, entry_iterator;
  struct photon_ri_ledger_entry_t tmp_entry;
  int ret, count, curr, curr_eager, still_searching;
  bool eager = false;

  dbg_trace("(%d, %d)", proc, tag);

  curr = photon_processes[proc].local_snd_info_ledger->curr;
  curr_entry = &(photon_processes[proc].local_snd_info_ledger->entries[curr]);

  curr_eager = photon_processes[proc].local_eager_ledger->curr;
  eager_entry = &(photon_processes[proc].local_eager_ledger->entries[curr_eager]);

  dbg_trace("Spinning on info/eager ledger looking for receive request");
  dbg_trace("looking in position %d/%p (%d/%p)", curr, curr_entry, curr_eager, eager_entry);

  count = 1;
  still_searching = 1;
  entry_iterator = curr_entry;
  /* TODO:  clean up this hacked loop */
  do {
    while((entry_iterator->header == 0 || entry_iterator->footer == 0) && (eager_entry->request == 0)) {
      ;
    }
    if (eager_entry->request && (size == PHOTON_ANY_SIZE)) {
      still_searching = 0;
      eager = true;
    }
    else if (eager_entry->request && (size == eager_entry->request>>32)) {
      still_searching = 0;
      eager = true;
    }
    if (still_searching) {
      if( ((tag < 0) || (entry_iterator->tag == tag )) && (size == PHOTON_ANY_SIZE) ) {
	still_searching = 0;
      }
      else if (((tag < 0) || (entry_iterator->tag == tag )) && (size == entry_iterator->size)) {
	still_searching = 0;
      }
      else {
	curr = (photon_processes[proc].local_snd_info_ledger->curr + count++) % photon_processes[proc].local_snd_info_ledger->num_entries;
	entry_iterator = &(photon_processes[proc].local_snd_info_ledger->entries[curr]);
      }
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

  if (request != NULL) {
    if (eager) {
      ret = __photon_setup_request_ledger_eager(eager_entry, curr_eager, proc, request);
    }
    else {
      ret = __photon_setup_request_ledger_info(curr_entry, curr, proc, request);
    }
    if (ret != PHOTON_OK) {
      log_err("Could not setup request");
      goto error_exit;
    }
  }

  if (eager) {
    NEXT_LEDGER_ENTRY(photon_processes[proc].local_eager_ledger);
    dbg_trace("new curr == %d", photon_processes[proc].local_eager_ledger->curr);
  }
  else {
    NEXT_LEDGER_ENTRY(photon_processes[proc].local_snd_info_ledger);
    dbg_trace("new curr == %d", photon_processes[proc].local_snd_info_ledger->curr);
  }

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
  int curr, still_searching;

  dbg_trace("(%d)", tag);

  dbg_trace("Spinning on send info ledger looking for send request");

  still_searching = 1;
  iproc = -1;
#ifdef DEBUG
  stime = time(NULL);
#endif
  do {
    iproc = (iproc+1)%_photon_nproc;
    curr = photon_processes[iproc].local_snd_info_ledger->curr;
    curr_entry = &(photon_processes[iproc].local_snd_info_ledger->entries[curr]);
    dbg_trace("looking in position %d/%p for proc %d", curr, curr_entry,iproc);

    count = 1;
    entry_iterator = curr_entry;
    // Some peers (procs) might have sent more than one send requests using different tags, so check them all.
    while(entry_iterator->header == 1 && entry_iterator->footer == 1) {
      if( (entry_iterator->addr == (uintptr_t)0) && (entry_iterator->priv.key0 == 0) && ((tag < 0) || (entry_iterator->tag == tag )) ) {
        still_searching = 0;
        dbg_trace("Found matching send request with tag %d from proc %d", tag, iproc);
        break;
      }
      else {
        dbg_trace("Found non-matching send request with tag %d from proc %d", tag, iproc);
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

  NEXT_LEDGER_ENTRY(photon_processes[iproc].local_snd_info_ledger);
  dbg_trace("new curr == %d", photon_processes[iproc].local_snd_info_ledger->curr);

  return PHOTON_OK;
}

static int _photon_post_os_put(photon_rid request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset) {
  photonRequest req;
  photonBI drb;
  photonBI db;
  int rc;

  dbg_trace("(%d, %p, %lu, %lu, %lu)", proc, ptr, size, r_offset, request);

  if (htable_lookup(reqtable, request, (void**)&req) != 0) {
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
    log_err("Requested to send %lu bytes to a buffer of size %lu at offset %lu", size, drb->buf.size, r_offset);
    goto error_exit;
  }

  dbg_trace("Posting Request ID: %d/%lu", proc, request);

  {
    rc = __photon_backend->rdma_put(proc, (uintptr_t)ptr, drb->buf.addr + (uintptr_t)r_offset,
                                    size, &(db->buf), &(drb->buf), request, 0);

    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT failed for 0x%016lx", request);
      goto error_exit;
    }
  }

  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}

static int _photon_post_os_get(photon_rid request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset) {
  photonRequest req;
  photonBI drb;
  photonBI db;
  int rc;

  dbg_trace("(%d, %p, %lu, %lu, 0x%016lx)", proc, ptr, size, r_offset, request);

  if (htable_lookup(reqtable, request, (void**)&req) != 0) {
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

  if (req->flags & REQUEST_FLAG_EAGER) {
    photonEagerBuf eb = photon_processes[proc].local_eager_buf;
    if ((eb->offset + size) > _photon_ebsize)
      NEXT_EAGER_BUF(eb, size);
    dbg_trace("EAGER copy message of size %lu from addr: 0x%016lx", size, (uintptr_t)&eb->data[eb->offset]);
    memcpy(ptr, &eb->data[eb->offset], size);
    memset(&eb->data[eb->offset], 0, size);
    NEXT_EAGER_BUF(eb, size);
    //req->state = REQUEST_COMPLETED;
    req->flags |= REQUEST_FLAG_EDONE;
    return PHOTON_OK;
  }

  dbg_trace("Posted Request ID: %d/0x%016lx", proc, request);

  {
    rc = __photon_backend->rdma_get(proc, (uintptr_t)ptr, drb->buf.addr + (uintptr_t)r_offset,
                                    size, &(db->buf), &(drb->buf), request, 0);

    if (rc != PHOTON_OK) {
      dbg_err("RDMA GET failed for 0x%016lx\n", request);
      goto error_exit;
    }
  }

  return PHOTON_OK;
  
 error_exit:
  return PHOTON_ERROR;
}

static int _photon_post_os_put_direct(int proc, void *ptr, uint64_t size, photonBuffer rbuf, int flags, photon_rid *request) {
  photonBI db;
  int rc, rflags;
  photon_rid request_id, event_id;

  dbg_trace("(%d, %p, %lu, %lu, %p)", proc, ptr, size, rbuf->size, request);

  if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
    log_err("Tried posting a os_put_direct() into a buffer that's not registered");
    return -1;
  }

  event_id = (( (uint64_t)proc)<<32) | INC_COUNTER(curr_cookie);
  dbg_trace("Incrementing curr_cookie_count to: 0x%016lx", event_id);

  if ((flags & PHOTON_REQ_USERID) && request) {
    request_id = *request;
    rflags = REQUEST_FLAG_USERID;
  }
  else {
    request_id = event_id;
    *request = event_id;
    rflags = REQUEST_FLAG_NIL;
  }

  {
    rc = __photon_backend->rdma_put(proc, (uintptr_t)ptr, rbuf->addr,
                                    rbuf->size, &(db->buf), rbuf, event_id, 0);
    
    if (rc != PHOTON_OK) {
      dbg_err("RDMA GET failed for 0x%016lx", event_id);
      goto error_exit;
    }

    dbg_trace("Posted Request/Event ID: %d/0x%016lx/0x%016lx", proc, request_id, event_id);
  }

  if (request != NULL) {
    rc = __photon_setup_request_direct(rbuf, proc, rflags, 1, request_id, event_id);
    if (rc != PHOTON_OK) {
      dbg_trace("Could not setup direct buffer request");
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

static int _photon_post_os_get_direct(int proc, void *ptr, uint64_t size, photonBuffer rbuf, int flags, photon_rid *request) {
  photonBI db;
  int rc, rflags;
  photon_rid request_id, event_id;

  dbg_trace("(%d, %p, %lu, %lu, %p)", proc, ptr, size, rbuf->size, request);

  if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
    log_err("Tried posting a os_get_direct() into a buffer that's not registered");
    return -1;
  }

  event_id = (( (uint64_t)proc)<<32) | INC_COUNTER(curr_cookie);
  dbg_trace("Incrementing curr_cookie_count to: 0x%016lx", event_id);

  if ((flags & PHOTON_REQ_USERID) && request) {
    request_id = *request;
    rflags = REQUEST_FLAG_USERID;
  }
  else {
    request_id = event_id;
    *request = event_id;
    rflags = REQUEST_FLAG_NIL;
  }

  {
    rc = __photon_backend->rdma_get(proc, (uintptr_t)ptr, rbuf->addr, size,
                                    &(db->buf), rbuf, event_id, 0);

    if (rc != PHOTON_OK) {
      dbg_err("RDMA GET failed for 0x%016lx", event_id);
      goto error_exit;
    }
    
    dbg_trace("Posted Request/Event ID: %d/0x%016lx/0x%016lx", proc, request_id, event_id);
  }

  if (request != NULL) {
    rc = __photon_setup_request_direct(rbuf, proc, rflags, 1, request_id, event_id);
    if (rc != PHOTON_OK) {
      dbg_trace("Could not setup direct buffer request");
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

static int _photon_send_FIN(photon_rid request, int proc, int flags) {
  photonRequest req;
  photonLedgerEntry entry;
  int curr, rc;

  dbg_trace("(%d)", proc);

  if (htable_lookup(reqtable, request, (void**)&req) != 0) {
    dbg_trace("Could not find request: 0x%016lx", request);
    goto error_exit;
  }

  if (req->state != REQUEST_COMPLETED) {
    dbg_trace("Warning: sending FIN for a request (EVQUEUE) that has not yet completed");
  }
  
  if (req->remote_buffer.request == NULL_COOKIE) {
    log_err("Trying to FIN a remote buffer request that was never set!");
    goto error_exit;
  }

  curr = photon_processes[proc].remote_fin_ledger->curr;
  entry = &photon_processes[proc].remote_fin_ledger->entries[curr];
  dbg_trace("photon_processes[%d].remote_fin_ledger->curr==%d", proc, curr);
  
  if( entry == NULL ) {
    log_err("entry is NULL for proc=%d", proc);
    goto error_exit;
  }
  
  entry->request = (uint64_t)req->remote_buffer.request;

  {
    uintptr_t rmt_addr;
    rmt_addr  = photon_processes[proc].remote_fin_ledger->remote.addr;
    rmt_addr += photon_processes[proc].remote_fin_ledger->curr * sizeof(*entry);

    rc = __photon_backend->rdma_put(proc, (uintptr_t)entry, rmt_addr, sizeof(*entry), &(shared_storage->buf),
                                    &(photon_processes[proc].remote_fin_ledger->remote), (photon_rid)NULL_COOKIE, 0);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA PUT failed for 0x%016lx", (photon_rid)NULL_COOKIE);
      goto error_exit;
    }
  }

  NEXT_LEDGER_ENTRY(photon_processes[proc].remote_fin_ledger);

  if (req->state == REQUEST_COMPLETED || flags & PHOTON_REQ_COMPLETED) {
    dbg_trace("Removing request 0x%016lx for remote buffer request 0x%016lx", request, req->remote_buffer.request);
    htable_remove(reqtable, req->id, NULL);
    SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
    dbg_trace("%d requests left in reqtable", htable_count(reqtable));
  }
  else {
    req->flags = REQUEST_FLAG_FIN;
    req->remote_buffer.request = NULL_COOKIE;
  }

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

// Polls EVQ waiting for an event.
// Returns the request associated with the event, but only removes request
// if it is an EVQUEUE event, not LEDGER.
// Can also return if an event not associated with a pending request was popped.
static int _photon_wait_any(int *ret_proc, photon_rid *ret_req) {
  int rc;

  dbg_trace("remaining: %d", htable_count(reqtable));

  if (ret_req == NULL) {
    goto error_exit;
  }

  while(1) {
    photon_rid cookie;
    int existed = -1;
    photon_event_status event;

    rc = __photon_backend->get_event(&event);
    if (rc < 0) {
      dbg_err("Error getting event");
      goto error_exit;
    }
    else if (rc != PHOTON_OK) {
      continue;
    }

    cookie = event.id;
    if (cookie != (photon_rid)NULL_COOKIE) {
      photonRequest req = NULL;
      void *test;
      
      if (htable_lookup(reqtable, cookie, (void**)&req) == 0) {
        if (req->type == EVQUEUE) {
          dbg_trace("setting request completed with cookie: 0x%016lx", cookie);
          req->state = REQUEST_COMPLETED;
        }
      }
      else if (htable_lookup(pwc_reqtable, cookie, (void**)&req) == 0) {
        if (req->type == EVQUEUE && (--req->num_entries) == 0) {
          dbg_trace("setting pwc request completed with cookie: 0x%016lx", cookie);
          req->state = REQUEST_COMPLETED;
        }
      }

      if (req && req->type == EVQUEUE && req->state == REQUEST_COMPLETED) {
        dbg_trace("removing event with cookie: 0x%016lx", cookie);
        existed = htable_remove(reqtable, cookie, &test);
        SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
      }
      else if (req) {
        existed = 1;
      }
    }

    if (existed == -1) {
      *ret_req = UINT64_MAX;
      *ret_proc = (uint32_t)(event.id>>32);
      return PHOTON_OK;
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

static int _photon_wait_any_ledger(int *ret_proc, photon_rid *ret_req) {
  static int i = -1; // this is static so we don't starve events in later processes
  int curr;

  dbg_trace("remaining: %d", htable_count(reqtable));

  if (ret_req == NULL || ret_proc == NULL) {
    goto error_exit;
  }

  if (htable_count(reqtable) == 0) {
    log_err("No events on queue to wait_one()");
    goto error_exit;
  }

  while(1) {
    photonLedgerEntry curr_entry;
    int exists;

    i=(i+1)%_photon_nproc;

    // check if an event occurred on the RDMA end of things
    curr = photon_processes[i].local_fin_ledger->curr;
    curr_entry = &(photon_processes[i].local_fin_ledger->entries[curr]);

    if (curr_entry->request != (uint64_t) 0) {
      void *test;
      dbg_trace("Wait All In: %d/0x%016lx", photon_processes[i].local_fin_ledger->curr, curr_entry->request);

      exists = htable_remove(reqtable, (uint64_t)curr_entry->request, &test);
      if (exists != -1) {
        photonRequest req;
        req = test;
        *ret_req = curr_entry->request;
        *ret_proc = i;
        SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
        break;
      }

      curr_entry->request = 0;
      NEXT_LEDGER_ENTRY(photon_processes[i].local_fin_ledger);
      dbg_trace("Wait All Out: %d", photon_processes[i].local_fin_ledger->curr);
    }
  }

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int _photon_probe_ledger(int proc, int *flag, int type, photonStatus status) {
  photonRILedger ledger;
  photonLedgerEntry eager_entry;
  photonRILedgerEntry entry_iterator;
  int i;
  int start, end, curr;

  //dbg_trace("(%d, %d)", proc, type);

  *flag = 0;

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

    //for (j=0; j<ledger->num_entries; j++) {
    {
      // process any eager entry first
      if (type == PHOTON_SEND_LEDGER) {
        curr = photon_processes[i].local_eager_ledger->curr;
        eager_entry = &(photon_processes[i].local_eager_ledger->entries[curr]);
        if (eager_entry->request) {
          status->src_addr.global.proc_id = i;
          status->request = eager_entry->request;
          status->size = eager_entry->request>>32;

          *flag = 1;
   
          return PHOTON_OK;
        }
      }
      
      entry_iterator = &(ledger->entries[ledger->curr]);
      if (entry_iterator->header && entry_iterator->footer && (entry_iterator->tag > 0)) {
        status->src_addr.global.proc_id = i;
        status->request = entry_iterator->request;
        status->tag = entry_iterator->tag;
        status->size = entry_iterator->size;
	
        dbg_trace("Request: 0x%016lx", entry_iterator->request);
        dbg_trace("Address: %p", (void *)entry_iterator->addr);
        dbg_trace("Size: %lu", entry_iterator->size);
        dbg_trace("Tag: %d", entry_iterator->tag);

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
  //dbg_trace("(%s)", buf);

  photonRequest req;
  int rc;
 
  req = SLIST_FIRST(&pending_recv_list);
  if (req) {
    //SAFE_SLIST_REMOVE_HEAD(&pending_recv_list, slist);
    *flag = 1;
    status->src_addr.global.proc_id = req->proc;
    status->request = req->id;
    status->tag = req->tag;
    status->size = req->length;
    status->count = 1;
    status->error = 0;
    dbg_trace("returning 0, flag:1");
    return PHOTON_OK;
  }
  else {
    *flag = 0;
    rc = __photon_nbpop_sr(NULL);
    //dbg_trace("returning %d, flag:0", rc);
    return rc;
  }
}

/* begin I/O */
static int _photon_io_init(char *file, int amode, void *view, int niter) {
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
/* end I/O */

/* TODO */
#ifdef PHOTON_MULTITHREADED
static inline int __photon_complete_ledger_req(photon_rid cookie) {
  photonRequest tmp_req;

  if (htable_lookup(reqtable, (uint64_t)cookie, (void**)&tmp_req) != 0)
    return -1;

  dbg_trace("completing ledger req %"PRIx32, cookie);
  pthread_mutex_lock(&tmp_req->mtx);
  {
    tmp_req->state = REQUEST_COMPLETED;
    SAFE_LIST_INSERT_HEAD(&unreaped_ledger_reqs_list, tmp_req, list);
    pthread_cond_broadcast(&tmp_req->completed);
  }
  pthread_mutex_unlock(&tmp_req->mtx);

  return 0;
}

static inline int __photon_complete_evd_req(photon_rid cookie) {
  photonRequest tmp_req;

  if (htable_lookup(reqtable, (uint64_t)cookie, (void**)&tmp_req) != 0)
    return -1;

  dbg_trace("completing event req %"PRIx32, cookie);
  pthread_mutex_lock(&tmp_req->mtx);
  {
    tmp_req->state = REQUEST_COMPLETED;
    SAFE_LIST_INSERT_HEAD(&unreaped_evd_reqs_list, tmp_req, list);
    pthread_cond_broadcast(&tmp_req->completed);
  }
  pthread_mutex_unlock(&tmp_req->mtx);

  return 0;
}

static void *__photon_event_watcher(void *arg) {
  int rc;
  uint32_t prefix;
  photon_event_status event;

  while(1) {
    //should get more events per call here
    rc = __photon_backend->get_event(&event);
    if (rc < 0) {
      dbg_err("Error getting event, rc=%d", rc);
      goto error_exit;
    }
    else if (rc != PHOTON_OK) {
      continue;
    }

    prefix = (uint32_t)(event.id>>32);

    if (prefix == REQUEST_COOK_RECV) {
      __photon_handle_recv_event(event.id);
    }
    else {
      __photon_handle_send_event(NULL, event.id);
    }    
  }

 error_exit:
  pthread_exit(NULL);
}

static void *__photon_req_watcher(void *arg) {
  int i, rc;
  int curr;
  uint32_t cookie;
  photon_event_status event;

  dbg_trace("reqs watcher started");

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
      photon_rdma_fin_ledger_entry_t *curr_entry;
      curr = photon_processes[i].local_fin_ledger->curr;
      curr_entry = &(photon_processes[i].local_fin_ledger->entries[curr]);
      if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
        dbg_trace("found: %d/%u", curr, curr_entry->request);
        curr_entry->header = 0;
        curr_entry->footer = 0;

        if (__photon_complete_ledger_req(curr_entry->request))
          log_err("couldn't find req for FIN ledger: %u", curr_entry->request);

        photon_processes[i].local_fin_ledger->curr = (photon_processes[i].local_fin_ledger->curr + 1) % photon_processes[i].local_fin_ledger->num_entries;
        dbg_trace("%d requests left in reqtable", htable_count(reqtable));
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
      !(addr->blkaddr.blk2)) {
    if (__photon_config->ibv.ud_gid_prefix) {
      inet_pton(AF_INET6, __photon_config->ibv.ud_gid_prefix, raddr->raw);
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
    dbg_err("Could not find buffer: 0x%016lx of size %lu", (uintptr_t)buf, size);
    return PHOTON_ERROR;
  }
}

int _photon_get_buffer_remote(photon_rid request, photonBuffer ret_buf) {
  photonRequest req;

  if (htable_lookup(reqtable, (uint64_t)request, (void**)&req) != 0) {
    dbg_trace("Could not find request: 0x%016lx", request);
    goto error_exit;
  }

  if ((req->state != REQUEST_NEW) && (req->state != REQUEST_PENDING)) {
    dbg_trace("Request has already trasitioned, can not return remote buffer info");
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
