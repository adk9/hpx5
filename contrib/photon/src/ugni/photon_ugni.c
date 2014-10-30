#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>

#include "photon_backend.h"
#include "photon_buffer.h"
#include "photon_exchange.h"

#include "photon_ugni.h"
#include "photon_ugni_connect.h"
#include "logging.h"
#include "utility_functions.h"

#define MAX_RETRIES 1
#define DEF_UGNI_BTE_THRESH 8192

struct rdma_args_t {
  int proc;
  uint64_t id;
  uint64_t laddr;
  uint64_t raddr;
  uint64_t size;
  gni_mem_handle_t lmdh;
  gni_mem_handle_t rmdh;
};

typedef struct photon_gni_descriptor_t {
  int curr;
  gni_post_descriptor_t *entries;
} photon_gni_descriptor;

static int __initialized = 0;

static int ugni_initialized(void);
static int ugni_init(photonConfig cfg, ProcessInfo *photon_processes, photonBI ss);
static int ugni_finalize(void);
static int ugni_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                         photonBuffer lbuf, photonBuffer rbuf, uint64_t id, int flags);
static int ugni_rdma_get(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                         photonBuffer lbuf, photonBuffer rbuf, uint64_t id, int flags);
static int ugni_rdma_send(photonAddr addr, uintptr_t laddr, uint64_t size,
                          photonBuffer lbuf, uint64_t id, int flags);
static int ugni_rdma_recv(photonAddr addr, uintptr_t laddr, uint64_t size,
                          photonBuffer lbuf, uint64_t id, int flags);
static int ugni_get_event(photonEventStatus stat);

static int __ugni_do_rdma(struct rdma_args_t *args, int opcode, int flags);
static int __ugni_do_fma(struct rdma_args_t *args, int opcode, int flags);

static ugni_cnct_ctx ugni_ctx;
static photon_gni_descriptor *descriptors;

/* we are now a Photon backend */
struct photon_backend_t photon_ugni_backend = {
  .context = &ugni_ctx,
  .initialized = ugni_initialized,
  .init = ugni_init,
  .cancel = NULL,
  .finalize = ugni_finalize,
  /* API */
  .get_dev_addr = NULL,
  .register_addr = NULL,
  .unregister_addr = NULL,
  .register_buffer = NULL,
  .unregister_buffer = NULL,
  .test = NULL,
  .wait = NULL,
  .wait_ledger = NULL,
  .send = NULL,
  .recv = NULL,
  .post_recv_buffer_rdma = NULL,
  .post_send_buffer_rdma = NULL,
  .post_send_request_rdma = NULL,
  .wait_recv_buffer_rdma = NULL,
  .wait_send_buffer_rdma = NULL,
  .wait_send_request_rdma = NULL,
  .post_os_put = NULL,
  .post_os_get = NULL,
  .send_FIN = NULL,
  .probe_ledger = NULL,
  .probe = NULL,
  .wait_any = NULL,
  .wait_any_ledger = NULL,
  .io_init = NULL,
  .io_finalize = NULL,
  /* data movement */
  .rdma_put = ugni_rdma_put,
  .rdma_get = ugni_rdma_get,
  .rdma_send = ugni_rdma_send,
  .rdma_recv = ugni_rdma_recv,
  .get_event = ugni_get_event
};

static int ugni_initialized() {
  if (__initialized == 1)
    return PHOTON_OK;
  else
    return PHOTON_ERROR_NOINIT;
}

static int ugni_init(photonConfig cfg, ProcessInfo *photon_processes, photonBI ss) {
  int i;
  // __initialized: 0 - not; -1 - initializing; 1 - initialized
  __initialized = -1;

  if (cfg->ugni.bte_thresh < 0)
    cfg->ugni.bte_thresh = DEF_UGNI_BTE_THRESH;
  else if (cfg->ugni.bte_thresh == 0)
    cfg->ugni.bte_thresh = INT_MAX;

  if (cfg->ugni.eth_dev) {
    ugni_ctx.gemini_dev = cfg->ugni.eth_dev;
  }
  else {
    ugni_ctx.gemini_dev = "ipogif0";
  }

  if(__ugni_init_context(&ugni_ctx)) {
    log_err("Could not initialize ugni context");
    goto error_exit;
  }

  if(__ugni_connect_peers(&ugni_ctx)) {
    log_err("Could not connect peers");
    goto error_exit;
  }

  if (photon_buffer_register(ss, &ugni_ctx) != 0) {
    log_err("couldn't register local buffer for the ledger entries");
    goto error_exit;
  }

  if (photon_exchange_ledgers(photon_processes, LEDGER_ALL) != PHOTON_OK) {
    log_err("couldn't exchange ledgers");
    goto error_exit;
  }

  // initialize the available descriptors
  descriptors = (photon_gni_descriptor*)calloc(_photon_nproc, sizeof(photon_gni_descriptor));
  for (i=0; i<_photon_nproc; i++) {
    descriptors[i].entries = (gni_post_descriptor_t*)calloc(_LEDGER_SIZE, sizeof(gni_post_descriptor_t));
  }

  __initialized = 1;

  dbg_trace("ended successfully =============");

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int ugni_finalize() {
  int i;
  /* should clean up allocated buffers */
  for (i=0; i<_photon_nproc; i++) {
    free(descriptors[i].entries);
  }
  free(descriptors);
  return PHOTON_OK;
}

static int __ugni_do_rdma(struct rdma_args_t *args, int opcode, int flags) {
  gni_post_descriptor_t *fma_desc;
  int err, curr;

  curr = descriptors[args->proc].curr;
  fma_desc = &(descriptors[args->proc].entries[curr]);
  descriptors[args->proc].curr = (descriptors[args->proc].curr + 1) % _LEDGER_SIZE;

  if (flags & RDMA_FLAG_NO_CQE) {
    fma_desc->cq_mode = GNI_CQMODE_SILENT;
    fma_desc->src_cq_hndl = NULL;
  }
  else {
    fma_desc->cq_mode = GNI_CQMODE_LOCAL_EVENT;
    fma_desc->src_cq_hndl = ugni_ctx.local_cq_handle;
  }

  fma_desc->type = opcode;
  fma_desc->dlvr_mode = GNI_DLVMODE_PERFORMANCE;
  fma_desc->local_addr = args->laddr;
  fma_desc->local_mem_hndl = args->lmdh;
  fma_desc->remote_addr = args->raddr;
  fma_desc->remote_mem_hndl = args->rmdh;
  fma_desc->length = args->size;
  fma_desc->post_id = args->id;
  fma_desc->rdma_mode = 0;

  err = GNI_PostRdma(ugni_ctx.ep_handles[args->proc], fma_desc);
  if (err != GNI_RC_SUCCESS) {
    log_err("GNI_PostRdma data ERROR status: %s (%d)\n", gni_err_str[err], err);
    goto error_exit;
  }
  dbg_trace("GNI_PostRdma data transfer successful: %"PRIx64, args->id);

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int __ugni_do_fma(struct rdma_args_t *args, int opcode, int flags) {
  gni_post_descriptor_t *fma_desc;
  int err, curr;
  
  curr = descriptors[args->proc].curr;
  fma_desc = &(descriptors[args->proc].entries[curr]);
  descriptors[args->proc].curr = (descriptors[args->proc].curr + 1) % _LEDGER_SIZE;

  if (flags & RDMA_FLAG_NO_CQE) {
    fma_desc->cq_mode = GNI_CQMODE_SILENT;
    fma_desc->src_cq_hndl = NULL;
  }
  else {
    fma_desc->cq_mode = GNI_CQMODE_GLOBAL_EVENT;
    fma_desc->src_cq_hndl = ugni_ctx.local_cq_handle;
  }

  fma_desc->type = opcode;
  fma_desc->dlvr_mode = GNI_DLVMODE_PERFORMANCE;
  fma_desc->local_addr = args->laddr;
  fma_desc->local_mem_hndl = args->lmdh;
  fma_desc->remote_addr = args->raddr;
  fma_desc->remote_mem_hndl = args->rmdh;
  fma_desc->length = args->size;
  fma_desc->post_id = args->id;
  fma_desc->rdma_mode = 0;

  err = GNI_PostFma(ugni_ctx.ep_handles[args->proc], fma_desc);
  if (err != GNI_RC_SUCCESS) {
    log_err("GNI_PostFma data ERROR status: %s (%d)", gni_err_str[err], err);
    goto error_exit;
  }
  dbg_trace("GNI_PostFma data transfer successful: %"PRIx64, args->id);

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int ugni_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                         photonBuffer lbuf, photonBuffer rbuf, uint64_t id, int flags) {
  struct rdma_args_t args;
  args.proc = proc;
  args.id = id;
  args.laddr = (uint64_t)laddr;
  args.raddr = (uint64_t)raddr;
  args.size = size;
  args.lmdh.qword1 = lbuf->priv.key0;
  args.lmdh.qword2 = lbuf->priv.key1;
  args.rmdh.qword1 = rbuf->priv.key0;
  args.rmdh.qword2 = rbuf->priv.key1;

  if (size < __photon_config->ugni.bte_thresh)
    return __ugni_do_fma(&args, GNI_POST_FMA_PUT, flags);
  else
    return __ugni_do_rdma(&args, GNI_POST_RDMA_PUT, flags);
}

static int ugni_rdma_get(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                         photonBuffer lbuf, photonBuffer rbuf, uint64_t id, int flags) {
  struct rdma_args_t args;
  args.proc = proc;
  args.id = id;
  args.laddr = (uint64_t)laddr;
  args.raddr = (uint64_t)raddr;
  args.size = size;
  args.lmdh.qword1 = lbuf->priv.key0;
  args.lmdh.qword2 = lbuf->priv.key1;
  args.rmdh.qword1 = rbuf->priv.key0;
  args.rmdh.qword2 = rbuf->priv.key1;

  if (size < __photon_config->ugni.bte_thresh)
    return __ugni_do_fma(&args, GNI_POST_FMA_GET, flags);
  else
    return __ugni_do_rdma(&args, GNI_POST_RDMA_GET, flags);
}

static int ugni_rdma_send(photonAddr addr, uintptr_t laddr, uint64_t size,
                          photonBuffer lbuf, uint64_t id, int flags) {
  return PHOTON_OK;
}

static int ugni_rdma_recv(photonAddr addr, uintptr_t laddr, uint64_t size,
                          photonBuffer lbuf, uint64_t id, int flags) {
  return PHOTON_OK;
}

static int ugni_get_event(photonEventStatus stat) {
  gni_post_descriptor_t *event_post_desc_ptr;
  gni_cq_entry_t current_event;
  uint64_t cookie;
  int rc;

  rc = get_cq_event(ugni_ctx.local_cq_handle, 1, 0, &current_event);
  if (rc == 0) {
    rc = GNI_GetCompleted(ugni_ctx.local_cq_handle, current_event, &event_post_desc_ptr);
    if (rc != GNI_RC_SUCCESS) {
      dbg_err("GNI_GetCompleted  data ERROR status: %s (%d)", gni_err_str[rc], rc);
    }
  }
  else if (rc == 3) {
    /* nothing available */
    return 1;
  }
  else {
    /* rc == 2 is an overrun */
    dbg_err("Error getting CQ event: %d\n", rc);
  }

  cookie = event_post_desc_ptr->post_id;
  dbg_trace("received event with cookie:%"PRIx64, cookie);

  stat->id = cookie;
  stat->proc = 0x0;
  stat->priv = NULL;

  return PHOTON_OK;
}
