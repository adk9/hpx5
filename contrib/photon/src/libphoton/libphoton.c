#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "libphoton.h"
#include "photon_backend.h"
#include "photon_forwarder.h"
#include "logging.h"
#include "util.h"

#ifdef HAVE_VERBS
#include "verbs.h"
#endif

#ifdef HAVE_UGNI
#include "photon_ugni.h"
#endif

#ifdef HAVE_XSP
#include "photon_xsp_forwarder.h"
#endif

/* Globals */
photonConfig __photon_config = NULL;
photonBackend __photon_backend = NULL;
photonForwarder __photon_forwarder = NULL;

int _LEDGER_SIZE;
int _photon_myrank;
int _photon_nproc;
int _photon_nforw;
int _photon_fproc;
int _photon_ebsize;
int _photon_smsize;
int _photon_spsize;
int _forwarder;

#if defined(ENABLE_DEBUG) || defined(ENABLE_CALLTRACE)
int _photon_start_debugging = 1;
#endif

#if defined(ENABLE_CALLTRACE)
FILE *_phot_ofp;
#endif
/* END Globals */

/* this default beckend will do our ledger work */
static photonBackend __photon_default = &photon_default_backend;

/* return 1==initialized, 0==not initialized */
int photon_initialized() {
  int rc;
  if (!__photon_backend) {
    return 0;
  }
  rc = __photon_backend->initialized();
  if (rc == PHOTON_OK) {
    return 1;
  }
  else {
    return 0;
  }
}

int photon_init(photonConfig cfg) {
  photonBackend be = NULL;
  photonConfig lcfg = NULL;
  char *errmsg = "";

  /* copy the configuration */
  lcfg = calloc(1, sizeof(struct photon_config_t));
  if (!lcfg) {
    log_err("Could not allocate space for internal config!");
    return PHOTON_ERROR;
  }

  memcpy(lcfg, cfg, sizeof(struct photon_config_t));
  if (cfg->ibv.eth_dev)
    lcfg->ibv.eth_dev = strdup(cfg->ibv.eth_dev);
  if (cfg->ibv.ib_dev)
    lcfg->ibv.ib_dev = strdup(cfg->ibv.ib_dev);
  if (cfg->ibv.ud_gid_prefix)
    lcfg->ibv.ud_gid_prefix = strdup(cfg->ibv.ud_gid_prefix);
  if (cfg->backend)
    lcfg->backend = strdup(cfg->backend);

  /* track the config with a global */
  __photon_config = lcfg;

  /* we can override the default with the tech-specific backends,
     but we just use their RDMA methods for now */
  if (lcfg->backend != NULL) {
    if (!strncasecmp(lcfg->backend, "verbs", 10)) {
#ifdef HAVE_VERBS
      __photon_backend = be = &photon_verbs_backend;
      photon_buffer_init(&verbs_buffer_interface);
#else
      errmsg = "IB verbs backend";
      goto error_exit;
#endif
    }
    else if (!strncasecmp(lcfg->backend, "ugni", 10)) {
#ifdef HAVE_UGNI
      __photon_backend = be = &photon_ugni_backend;
      photon_buffer_init(&ugni_buffer_interface);
#else
      errmsg = "UGNI backend";
      goto error_exit;
#endif
    }
    else {
      errmsg = "unknown backend";
      goto error_exit;
    }
  }
  else {
    log_warn("photon_init(): backend not specified, using default test backend!");
  }

  switch (lcfg->meta_exch) {
  case PHOTON_EXCH_MPI:
#ifndef HAVE_MPI
    errmsg = "MPI exchange";
    goto error_exit;
#endif
    break;
  case PHOTON_EXCH_PMI:
#ifndef HAVE_PMI
    errmsg = "PMI exchange";
    goto error_exit;
#endif
    break;
  case PHOTON_EXCH_XSP:
#ifndef HAVE_XSP
    errmsg = "XSP exchange";
    goto error_exit;
#endif
    break;
  case PHOTON_EXCH_EXTERNAL:
    if ((lcfg->exch.allgather == NULL) || 
	(lcfg->exch.barrier == NULL)) {
      errmsg = "External exchange function (see config)";
      goto error_exit;
    } else {
      assert(lcfg->exch.allgather);
      assert(lcfg->exch.barrier);
    }
    break;
  default:
    errmsg = "unknown exchange";
    goto error_exit;
    break;
  }
  
  /* set globals */
  _photon_myrank = (int)lcfg->address;
  _photon_nproc = lcfg->nproc;
  _photon_nforw = lcfg->forwarder.use_forwarder;
  _photon_ebsize = lcfg->cap.eager_buf_size;
  _photon_smsize = lcfg->cap.small_msg_size;
  _photon_spsize = lcfg->cap.small_pwc_size;
  _LEDGER_SIZE = lcfg->cap.ledger_entries;
  
  /* update defaults */
  if (_photon_ebsize < 0)
    _photon_ebsize = DEF_EAGER_BUF_SIZE;
  if (_photon_smsize < 0)
    _photon_smsize = DEF_SMALL_MSG_SIZE;
  if (_LEDGER_SIZE <= 0)
    _LEDGER_SIZE = DEF_LEDGER_SIZE;
  if (lcfg->cap.max_rd < 0)
    lcfg->cap.max_rd = DEF_MAX_REQUESTS;
  if (lcfg->cap.default_rd <= 0)
    lcfg->cap.default_rd = DEF_NUM_REQUESTS;
  
  assert(is_power_of_2(_LEDGER_SIZE));
  assert(is_power_of_2(_photon_ebsize));
  
  /* figure out forwarder info */
  if (lcfg->forwarder.use_forwarder) {
    /* if init is called with a rank greater than or equal to nproc, then we are a forwarder
       TODO: fix this in the case of non-MPI */
    if ((int)lcfg->address >= lcfg->nproc) {
      _photon_fproc = (int)lcfg->address;
      _forwarder = 1;
    }
    /* otherwise we are a proc that wants to use a forwarder */
    else {
      /* do some magic to determing which forwarder to use for this rank
         right now every rank gets the first forwarder */
      _photon_fproc = lcfg->nproc;
      _forwarder = 0;
    }
#ifdef HAVE_XSP
    __photon_forwarder = &xsp_forwarder;
#else
    log_warn("Photon was built without forwarder support!");
#endif
  }

  /* check if the selected backend defines its own API methods and use those instead */
  __photon_default->cancel = (be->cancel)?(be->cancel):__photon_default->cancel;
  __photon_default->finalize = (be->finalize)?(be->finalize):__photon_default->finalize;
  __photon_default->register_buffer = (be->register_buffer)?(be->register_buffer):__photon_default->register_buffer;
  __photon_default->unregister_buffer = (be->unregister_buffer)?(be->unregister_buffer):__photon_default->unregister_buffer;
  __photon_default->get_dev_addr = (be->get_dev_addr)?(be->get_dev_addr):__photon_default->get_dev_addr;
  __photon_default->register_addr = (be->register_addr)?(be->register_addr):__photon_default->register_addr;
  __photon_default->unregister_addr = (be->unregister_addr)?(be->unregister_addr):__photon_default->unregister_addr;
  __photon_default->test = (be->test)?(be->test):__photon_default->test;
  __photon_default->wait = (be->wait)?(be->wait):__photon_default->wait;
  __photon_default->wait_ledger = (be->wait_ledger)?(be->wait_ledger):__photon_default->wait_ledger;
  __photon_default->post_recv_buffer_rdma = (be->post_recv_buffer_rdma)?(be->post_recv_buffer_rdma):__photon_default->post_recv_buffer_rdma;
  __photon_default->post_send_buffer_rdma = (be->post_send_buffer_rdma)?(be->post_send_buffer_rdma):__photon_default->post_send_buffer_rdma;
  __photon_default->post_send_request_rdma = (be->post_send_request_rdma)?(be->post_send_request_rdma):__photon_default->post_send_request_rdma;
  __photon_default->wait_recv_buffer_rdma = (be->wait_recv_buffer_rdma)?(be->wait_recv_buffer_rdma):__photon_default->wait_recv_buffer_rdma;
  __photon_default->wait_send_buffer_rdma = (be->wait_send_buffer_rdma)?(be->wait_send_buffer_rdma):__photon_default->wait_send_buffer_rdma;
  __photon_default->wait_send_request_rdma = (be->wait_send_request_rdma)?(be->wait_send_request_rdma):__photon_default->wait_send_request_rdma;
  __photon_default->post_os_put = (be->post_os_put)?(be->post_os_put):__photon_default->post_os_put;
  __photon_default->post_os_get = (be->post_os_get)?(be->post_os_get):__photon_default->post_os_get;
  __photon_default->post_os_put_direct = (be->post_os_put_direct)?(be->post_os_put_direct):__photon_default->post_os_put_direct;
  __photon_default->post_os_get_direct = (be->post_os_get_direct)?(be->post_os_get_direct):__photon_default->post_os_get_direct;
  __photon_default->send_FIN = (be->send_FIN)?(be->send_FIN):__photon_default->send_FIN;
  __photon_default->wait_any = (be->wait_any)?(be->wait_any):__photon_default->wait_any;
  __photon_default->wait_any_ledger = (be->wait_any_ledger)?(be->wait_any_ledger):__photon_default->wait_any_ledger;
  __photon_default->probe_ledger = (be->probe_ledger)?(be->probe_ledger):__photon_default->probe_ledger;
  __photon_default->probe = (be->probe)?(be->probe):__photon_default->probe;
  __photon_default->send = (be->send)?(be->send):__photon_default->send;
  __photon_default->recv = (be->recv)?(be->recv):__photon_default->recv;
  __photon_default->put_with_completion = (be->put_with_completion)?(be->put_with_completion):__photon_default->put_with_completion;
  __photon_default->get_with_completion = (be->get_with_completion)?(be->get_with_completion):__photon_default->get_with_completion;
  __photon_default->probe_completion = (be->probe_completion)?(be->probe_completion):__photon_default->probe_completion;
  __photon_default->io_init = (be->io_init)?(be->io_init):__photon_default->io_init;
  __photon_default->io_init = (be->io_finalize)?(be->io_finalize):__photon_default->io_finalize;

  if(__photon_backend->initialized() == PHOTON_OK) {
    log_warn("Photon already initialized");
    return PHOTON_OK;
  }

  dbg_info("Photon initializing");

  /* the configured backend init gets called from within the default library init */
  return __photon_default->init(lcfg, NULL, NULL);

error_exit:
  log_err("photon_init(): %s support not present", errmsg);
  free(lcfg);
  return PHOTON_ERROR;
}

int photon_cancel(photon_rid request, int flags) {
  int rc;
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }
  
  rc = __photon_default->cancel(request, flags);
  if (rc != PHOTON_OK) {
    dbg_err("Error in backend cancel");
    return rc;
  }

  return PHOTON_OK;
}

int photon_finalize() {
  int rc;
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  rc = __photon_default->finalize();
  if (rc != PHOTON_OK) {
    log_err("Error in backend finalize");
    return rc;
  }

  if (__photon_config->ibv.eth_dev)
    free(__photon_config->ibv.eth_dev);
  if (__photon_config->ibv.ib_dev)
    free(__photon_config->ibv.ib_dev);
  if (__photon_config->ibv.ud_gid_prefix)
    free(__photon_config->ibv.ud_gid_prefix);
  if (__photon_config->backend)
    free(__photon_config->backend);
  free(__photon_config);

  return PHOTON_OK;
}

int photon_register_buffer(void *buf, uint64_t size) {
  return __photon_default->register_buffer(buf, size);
}

int photon_unregister_buffer(void *buf, uint64_t size) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->unregister_buffer(buf, size);
}

int photon_get_dev_addr(int af, photonAddr addr) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->get_dev_addr(af, addr);
}

int photon_register_addr(photonAddr addr, int af) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->register_addr(addr, af);
}

int photon_unregister_addr(photonAddr addr, int af) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->unregister_addr(addr, af);
}

int photon_test(photon_rid request, int *flag, int *type, photonStatus status) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->test(request, flag, type, status);
}

int photon_wait(photon_rid request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->wait(request);
}

int photon_wait_ledger(photon_rid request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->wait_ledger(request);
}

int photon_post_recv_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, photon_rid *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_recv_buffer_rdma(proc, ptr, size, tag, request);
}

int photon_post_send_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, photon_rid *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_send_buffer_rdma(proc, ptr, size, tag, request);
}

int photon_post_send_request_rdma(int proc, uint64_t size, int tag, photon_rid *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_send_request_rdma(proc, size, tag, request);
}

int photon_wait_recv_buffer_rdma(int proc, uint64_t size, int tag, photon_rid *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->wait_recv_buffer_rdma(proc, size, tag, request);
}

int photon_wait_send_buffer_rdma(int proc, uint64_t size, int tag, photon_rid *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->wait_send_buffer_rdma(proc, size, tag, request);
}

int photon_wait_send_request_rdma(int tag) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->wait_send_request_rdma(tag);
}

int photon_post_os_put(photon_rid request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_os_put(request, proc, ptr, size, tag, r_offset);
}

int photon_post_os_get(photon_rid request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_os_get(request, proc, ptr, size, tag, r_offset);
}

int photon_post_os_put_direct(int proc, void *ptr, uint64_t size, photonBuffer rbuf, int flags, photon_rid *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_os_put_direct(proc, ptr, size, rbuf, flags, request);
}

int photon_post_os_get_direct(int proc, void *ptr, uint64_t size, photonBuffer rbuf, int flags, photon_rid *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_os_get_direct(proc, ptr, size, rbuf, flags, request);
}

int photon_send_FIN(photon_rid request, int proc, int flags) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->send_FIN(request, proc, flags);
}

int photon_wait_any(int *ret_proc, photon_rid *ret_req) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

#ifdef PHOTON_MULTITHREADED
  /* TODO: These can probably be implemented by having
     a condition var for the unreaped lists */
  return PHOTON_ERROR;
#else
  return __photon_default->wait_any(ret_proc, ret_req);
#endif
}

int photon_wait_any_ledger(int *ret_proc, photon_rid *ret_req) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

#ifdef PHOTON_MULTITHREADED
  return PHOTON_ERROR;
#else
  return __photon_default->wait_any_ledger(ret_proc, ret_req);
#endif
}

int photon_probe_ledger(int proc, int *flag, int type, photonStatus status) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->probe_ledger(proc, flag, type, status);
}


/* begin SR interface */
int photon_probe(photonAddr addr, int *flag, photonStatus status) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->probe(addr, flag, status);
}

int photon_send(photonAddr addr, void *ptr, uint64_t size, int flags, uint64_t *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->send(addr, ptr, size, flags, request);
}

int photon_recv(uint64_t request, void *ptr, uint64_t size, int flags) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->recv(request, ptr, size, flags);
}
/* end SR interface */

/* begin with completion */
int photon_put_with_completion(int proc, void *ptr, uint64_t size, void *rptr, struct photon_buffer_priv_t priv,
                               photon_rid local, photon_rid remote, int flags) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }
  
  return __photon_default->put_with_completion(proc, ptr, size, rptr, priv, local, remote, flags);
}

int photon_get_with_completion(int proc, void *ptr, uint64_t size, void *rptr, struct photon_buffer_priv_t priv,
                               photon_rid local, int flags) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }
  
  return __photon_default->get_with_completion(proc, ptr, size, rptr, priv, local, flags);
}

int photon_probe_completion(int proc, int *flag, int *remaining, photon_rid *request, int flags) {
  //if(__photon_default->initialized() != PHOTON_OK) {
  //  init_err();
  //  return PHOTON_ERROR_NOINIT;
  //}
  
  return __photon_default->probe_completion(proc, flag, remaining, request, flags);
}
/* end with completion */

/* begin I/O */
int photon_io_init(char *file, int amode, void *view, int niter) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->io_init(file, amode, view, niter);
}

int photon_io_finalize() {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->io_finalize();
}

void photon_io_print_info(void *io) {
  if (__photon_forwarder != NULL) {
    __photon_forwarder->io_print(io);
  }
}

/* end I/O */

/* utility API method to get backend-specific buffer info */
int photon_get_buffer_private(void *buf, uint64_t size, photonBufferPriv ret_priv) {
  return _photon_get_buffer_private(buf, size, ret_priv);
}

/* utility method to get the remote buffer info set after a wait buffer request */
int photon_get_buffer_remote(photon_rid request, photonBuffer ret_desc) {
  return _photon_get_buffer_remote(request, ret_desc);
}
