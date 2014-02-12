#include <strings.h>
#include "libphoton.h"
#include "logging.h"

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

int _photon_myrank;
int _photon_nproc;
int _photon_nforw;
int _photon_fproc;
int _forwarder;

#ifdef DEBUG
int _photon_start_debugging=1;
#endif

#if defined(DEBUG) || defined(CALLTRACE)
FILE *_phot_ofp;
#endif
/* END Globals */

/* this default beckend will do our ledger work */
static photonBackend __photon_default = &photon_default_backend;

int photon_init(photonConfig cfg) {
  photonBackend be;

  /* we can override the default with the tech-specific backends,
     but we just use their RDMA methods for now */
  if (cfg->backend != NULL) {
    if (!strncasecmp(cfg->backend, "verbs", 10)) {
#ifdef HAVE_VERBS
      __photon_backend = be = &photon_verbs_backend;
      photon_buffer_init(&verbs_buffer_interface);
#else
      goto error_exit;
#endif
    }
    else if (!strncasecmp(cfg->backend, "ugni", 10)) {
#ifdef HAVE_UGNI
      __photon_backend = be = &photon_ugni_backend;
      photon_buffer_init(&ugni_buffer_interface);
#else
      goto error_exit;
#endif
    }
    else {
      goto error_exit;
    }
  }
  else {
    log_warn("photon_init(): backend not specified, using default test backend!");
  }

  /* set globals */
  _photon_nproc = cfg->nproc;
  _photon_nforw = cfg->use_forwarder;
  _photon_myrank = (int)cfg->address;

  /* figure out forwarder info */
  if (cfg->use_forwarder) {
    /* if init is called with a rank greater than or equal to nproc, then we are a forwarder
       TODO: fix this in the case of non-MPI */
    if ((int)cfg->address >= cfg->nproc) {
      _photon_fproc = (int)cfg->address;
      _forwarder = 1;
    }
    /* otherwise we are a proc that wants to use a forwarder */
    else {
      /* do some magic to determing which forwarder to use for this rank
         right now every rank gets the first forwarder */
      _photon_fproc = cfg->nproc;
      _forwarder = 0;
    }
#ifdef HAVE_XSP
    __photon_forwarder = &xsp_forwarder;
#else
    log_warn("No forwarder enabled!");
#endif
  }

  __photon_config = cfg;

  /* check if the selected backend defines its own API methods and use those instead */
  __photon_default->finalize = (be->finalize)?(be->finalize):__photon_default->finalize;
  __photon_default->register_buffer = (be->register_buffer)?(be->register_buffer):__photon_default->register_buffer;
  __photon_default->unregister_buffer = (be->unregister_buffer)?(be->unregister_buffer):__photon_default->unregister_buffer;
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
  __photon_default->io_init = (be->io_init)?(be->io_init):__photon_default->io_init;
  __photon_default->io_finalize = (be->io_finalize)?(be->io_finalize):__photon_default->io_finalize;

  if(__photon_backend->initialized() == PHOTON_OK) {
    log_warn("Photon already initialized");
    return PHOTON_OK;
  }

  /* the configured backend init gets called from within the default library init */
  return __photon_default->init(cfg, NULL, NULL);

error_exit:
  log_err("photon_init(): %s support not present", cfg->backend);
  return PHOTON_ERROR;
}

int photon_finalize() {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->finalize();
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

int photon_test(uint32_t request, int *flag, int *type, photonStatus status) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->test(request, flag, type, status);
}

int photon_wait(uint32_t request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->wait(request);
}

int photon_wait_ledger(uint32_t request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->wait_ledger(request);
}

int photon_post_recv_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, uint32_t *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_recv_buffer_rdma(proc, ptr, size, tag, request);
}

int photon_post_send_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, uint32_t *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_send_buffer_rdma(proc, ptr, size, tag, request);
}

int photon_post_send_request_rdma(int proc, uint64_t size, int tag, uint32_t *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_send_request_rdma(proc, size, tag, request);
}

int photon_wait_recv_buffer_rdma(int proc, int tag, uint32_t *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->wait_recv_buffer_rdma(proc, tag, request);
}

int photon_wait_send_buffer_rdma(int proc, int tag, uint32_t *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->wait_send_buffer_rdma(proc, tag, request);
}

int photon_wait_send_request_rdma(int tag) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->wait_send_request_rdma(tag);
}

int photon_post_os_put(uint32_t request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_os_put(request, proc, ptr, size, tag, r_offset);
}

int photon_post_os_get(uint32_t request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_os_get(request, proc, ptr, size, tag, r_offset);
}

int photon_post_os_put_direct(int proc, void *ptr, uint64_t size, int tag, photonDescriptor rbuf, uint32_t *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_os_put_direct(proc, ptr, size, tag, rbuf, request);
}

int photon_post_os_get_direct(int proc, void *ptr, uint64_t size, int tag, photonDescriptor rbuf, uint32_t *request) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->post_os_get_direct(proc, ptr, size, tag, rbuf, request);
}

int photon_send_FIN(uint32_t request, int proc) {
  if(__photon_default->initialized() != PHOTON_OK) {
    init_err();
    return PHOTON_ERROR_NOINIT;
  }

  return __photon_default->send_FIN(request, proc);
}

int photon_wait_any(int *ret_proc, uint32_t *ret_req) {
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

int photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req) {
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

/* begin I/O */
int photon_io_init(char *file, int amode, MPI_Datatype view, int niter) {
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

/* utility API method to get backend-specific buffer info */
int photon_get_buffer_private(void *buf, uint64_t size, photonBufferPriv ret_priv) {
  return _photon_get_buffer_private(buf, size, ret_priv);
}

/* utility method to get the remote buffer info set after a wait buffer request */
int photon_get_buffer_remote_descriptor(uint32_t request, photonDescriptor ret_desc) {
  return _photon_get_buffer_remote_descriptor(request, ret_desc);
}
