#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "photon_backend.h"
#include "photon_buffer.h"
#include "photon_exchange.h"

#include "photon_ugni.h"
#include "photon_ugni_connect.h"
#include "logging.h"
#include "utility_functions.h"

#define MAX_RETRIES 1

struct rdma_args_t {
  int proc;
  uint64_t id;
  uint64_t laddr;
  uint64_t raddr;
  uint64_t size;
  gni_mem_handle_t lmdh;
  gni_mem_handle_t rmdh;
};

static int __initialized = 0;

static int ugni_initialized(void);
static int ugni_init(photonConfig cfg, ProcessInfo *photon_processes, photonBI ss);
static int ugni_finalize(void);
static int ugni_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                         photonBuffer lbuf, photonBuffer rbuf, uint64_t id);
static int ugni_rdma_get(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                         photonBuffer lbuf, photonBuffer rbuf, uint64_t id);
static int ugni_rdma_send(photonAddr addr, uintptr_t laddr, uint64_t size,
                          photonBuffer lbuf, uint64_t id);
static int ugni_rdma_recv(photonAddr addr, uintptr_t laddr, uint64_t size,
                          photonBuffer lbuf, uint64_t id);
static int ugni_get_event(photonEventStatus stat);

static int __ugni_do_rdma(struct rdma_args_t *args, int opcode);
static int __ugni_do_fma(struct rdma_args_t *args, int opcode);

static ugni_cnct_ctx ugni_ctx;

/* we are now a Photon backend */
struct photon_backend_t photon_ugni_backend = {
  .context = &ugni_ctx,
  .initialized = ugni_initialized,
  .init = ugni_init,
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

  // __initialized: 0 - not; -1 - initializing; 1 - initialized
  __initialized = -1;

  if (cfg->eth_dev) {
    ugni_ctx.gemini_dev = cfg->eth_dev;
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
  
  __initialized = 1;

  dbg_info("ended successfully =============");

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int ugni_finalize() {
  /* should clean up allocated buffers */
  return PHOTON_OK;
}

static int __ugni_do_rdma(struct rdma_args_t *args, int opcode) {
  gni_post_descriptor_t *fma_desc;
  int err;

  /* create a new descriptor, free once the event is completed
     this is different from verbs where the descriptors are copied in the call */
  fma_desc = (gni_post_descriptor_t *)calloc(1, sizeof(gni_post_descriptor_t));
  if (!fma_desc) {
    dbg_err("Could not allocate new post descriptor");
    goto error_exit;
  }

  fma_desc->type = opcode;
  fma_desc->cq_mode = GNI_CQMODE_GLOBAL_EVENT;
  fma_desc->dlvr_mode = GNI_DLVMODE_PERFORMANCE;
  fma_desc->local_addr = args->laddr;
  fma_desc->local_mem_hndl = args->lmdh;
  fma_desc->remote_addr = args->raddr;
  fma_desc->remote_mem_hndl = args->rmdh;
  fma_desc->length = args->size;
  fma_desc->post_id = args->id;
  fma_desc->rdma_mode = 0;
  fma_desc->src_cq_hndl = ugni_ctx.local_cq_handle;

  err = GNI_PostRdma(ugni_ctx.ep_handles[args->proc], fma_desc);
  if (err != GNI_RC_SUCCESS) {
    log_err("GNI_PostRdma data ERROR status: %s (%d)\n", gni_err_str[err], err);
    goto error_exit;
  }
  dbg_info("GNI_PostRdma data transfer successful: %"PRIx64, args->id);

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int __ugni_do_fma(struct rdma_args_t *args, int opcode) {
  gni_post_descriptor_t *fma_desc;
  int err;

  /* create a new descriptor, free once the event is completed
     this is different from verbs where the descriptors are copied in the call */
  fma_desc = (gni_post_descriptor_t *)calloc(1, sizeof(gni_post_descriptor_t));
  if (!fma_desc) {
    dbg_err("Could not allocate new post descriptor");
    goto error_exit;
  }

  fma_desc->type = opcode;
  fma_desc->cq_mode = GNI_CQMODE_GLOBAL_EVENT;
  fma_desc->dlvr_mode = GNI_DLVMODE_PERFORMANCE;
  fma_desc->local_addr = args->laddr;
  fma_desc->local_mem_hndl = args->lmdh;
  fma_desc->remote_addr = args->raddr;
  fma_desc->remote_mem_hndl = args->rmdh;
  fma_desc->length = args->size;
  fma_desc->post_id = args->id;
  fma_desc->rdma_mode = 0;
  fma_desc->src_cq_hndl = ugni_ctx.local_cq_handle;

  err = GNI_PostFma(ugni_ctx.ep_handles[args->proc], fma_desc);
  if (err != GNI_RC_SUCCESS) {
    log_err("GNI_PostFma data ERROR status: %s (%d)\n", gni_err_str[err], err);
    goto error_exit;
  }
  dbg_info("GNI_PostFma data transfer successful: %"PRIx64, args->id);

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int ugni_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                         photonBuffer lbuf, photonBuffer rbuf, uint64_t id) {
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
  return __ugni_do_fma(&args, GNI_POST_FMA_PUT);
}

static int ugni_rdma_get(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                         photonBuffer lbuf, photonBuffer rbuf, uint64_t id) {
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
  return __ugni_do_fma(&args, GNI_POST_FMA_GET);
}

static int ugni_rdma_send(photonAddr addr, uintptr_t laddr, uint64_t size,
                          photonBuffer lbuf, uint64_t id) {
  return PHOTON_OK;
}

static int ugni_rdma_recv(photonAddr addr, uintptr_t laddr, uint64_t size,
                          photonBuffer lbuf, uint64_t id) {
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
  else {
    /* rc == 2 is an overrun */
    dbg_err("Error getting CQ event: %d\n", rc);
  }

  cookie = event_post_desc_ptr->post_id;
  dbg_info("received event with cookie:%"PRIx64, cookie);

  stat->id = cookie;
  stat->proc = 0x0;
  stat->priv = NULL;

  /* free up the descriptor we allocated for the operation
     this might get freed internally by GNI... */
  if (event_post_desc_ptr) {
    //free(event_post_desc_ptr);
  }

  return PHOTON_OK;
}
