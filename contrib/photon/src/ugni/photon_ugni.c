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
#include "photon_event.h"

#include "photon_ugni.h"
#include "photon_ugni_connect.h"
#include "logging.h"
#include "utility_functions.h"
#include "libsync/locks.h"

#define MAX_RETRIES    1

struct rdma_args_t {
  int proc;
  uint64_t id;
  uint64_t laddr;
  uint64_t raddr;
  uint64_t size;
  gni_mem_handle_t lmdh;
  gni_mem_handle_t rmdh;
  uint32_t imm_data;
};

typedef struct photon_gni_descriptor_t {
  tatas_lock_t lock;
  uint64_t curr;
  gni_post_descriptor_t *entries;
} photon_gni_descriptor;

static tatas_lock_t cq_lock;
static int __initialized = 0;

static int ugni_initialized(void);
static int ugni_init(photonConfig cfg, ProcessInfo *photon_processes, photonBI ss);
static int ugni_finalize(void);
static int ugni_get_info(ProcessInfo *pi, int proc, void **info, int *size, photon_info_t type);
static int ugni_set_info(ProcessInfo *pi, int proc, void *info, int size, photon_info_t type);
static int ugni_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                         photonBuffer lbuf, photonBuffer rbuf, uint64_t id, uint64_t imm, int flags);
static int ugni_rdma_get(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                         photonBuffer lbuf, photonBuffer rbuf, uint64_t id, int flags);
static int ugni_rdma_send(photonAddr addr, uintptr_t laddr, uint64_t size,
                          photonBuffer lbuf, uint64_t id, uint64_t imm, int flags);
static int ugni_rdma_recv(photonAddr addr, uintptr_t laddr, uint64_t size,
                          photonBuffer lbuf, uint64_t id, int flags);
static int ugni_get_event(int proc, int max, photon_rid *ids, int *n);
static int ugni_get_revent(int proc, int max, photon_rid *ids, uint64_t *imms, int *n);

static int __ugni_do_rdma(struct rdma_args_t *args, int opcode, int flags);
static int __ugni_do_fma(struct rdma_args_t *args, int opcode, int flags);

static ugni_cnct_ctx ugni_ctx;
static photon_gni_descriptor *descriptors;

// we are now a Photon backend
struct photon_backend_t photon_ugni_backend = {
  .context = &ugni_ctx,
  .initialized = ugni_initialized,
  .init = ugni_init,
  .cancel = NULL,
  .finalize = ugni_finalize,
  .connect = NULL,
  .get_info = ugni_get_info,
  .set_info = ugni_set_info,
  // API
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
  // data movement
  .rdma_put = ugni_rdma_put,
  .rdma_get = ugni_rdma_get,
  .rdma_send = ugni_rdma_send,
  .rdma_recv = ugni_rdma_recv,
  .get_event = ugni_get_event,
  .get_revent = ugni_get_revent
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

  sync_tatas_init(&cq_lock);

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

  ugni_ctx.num_cq = cfg->cap.num_cq;
  ugni_ctx.use_rcq = cfg->cap.use_rcq;
  ugni_ctx.rdma_put_align = PHOTON_UGNI_PUT_ALIGN;
  ugni_ctx.rdma_get_align = PHOTON_UGNI_GET_ALIGN;

  if ((2 * _LEDGER_SIZE * _photon_nproc / ugni_ctx.num_cq) > MAX_CQ_ENTRIES) {
    one_warn("Possible CQ overrun with current config (nproc=%d, nledger=%d, ncq=%d)",
	     _photon_nproc, _LEDGER_SIZE, ugni_ctx.num_cq);
  }

  if(__ugni_init_context(&ugni_ctx)) {
    log_err("Could not initialize ugni context");
    goto error_exit;
  }

  if(__ugni_connect_peers(&ugni_ctx)) {
    log_err("Could not connect peers");
    goto error_exit;
  }

  if (photon_buffer_register(ss, &ugni_ctx, BUFFER_FLAG_NOTIFY) != 0) {
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
    descriptors[i].entries = (gni_post_descriptor_t*)calloc(MAX_CQ_ENTRIES, sizeof(gni_post_descriptor_t));
  }

  __initialized = 1;

  dbg_trace("ended successfully =============");

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int ugni_finalize() {
  int i;
  // should clean up allocated buffers
  for (i=0; i<_photon_nproc; i++) {
    free(descriptors[i].entries);
  }
  free(descriptors);
  return PHOTON_OK;
}

static int ugni_get_info(ProcessInfo *pi, int proc, void **ret_info, int *ret_size, photon_info_t type) {
  switch(type) {
  case PHOTON_GET_ALIGN:
    {
      *ret_info = &ugni_ctx.rdma_get_align;
      *ret_size = sizeof(ugni_ctx.rdma_get_align);
    }
    break;
  case PHOTON_PUT_ALIGN:
    {
      *ret_info = &ugni_ctx.rdma_put_align;
      *ret_size = sizeof(ugni_ctx.rdma_put_align);
    }
    break;
  default:
    goto error_exit;
    break;
  }
  
  return PHOTON_OK;

 error_exit:
  return PHOTON_ERROR;
}

static int ugni_set_info(ProcessInfo *pi, int proc, void *info, int size, photon_info_t type) {

  return PHOTON_OK;
}

static int __ugni_do_rdma(struct rdma_args_t *args, int opcode, int flags) {
  gni_post_descriptor_t *fma_desc;
  int err, curr, curr_ind, cqind;
  const int max_trials = 1000;
  int trials = 0;

  cqind = PHOTON_GET_CQ_IND(ugni_ctx.num_cq, args->proc);
  curr = sync_fadd(&descriptors[args->proc].curr, 1, SYNC_RELAXED);
  curr_ind = curr & (MAX_CQ_ENTRIES - 1);
  fma_desc = &(descriptors[args->proc].entries[curr_ind]);

  if (flags & RDMA_FLAG_NO_CQE) {
    fma_desc->cq_mode = GNI_CQMODE_SILENT;
    fma_desc->src_cq_hndl = NULL;
  }
  else {
    fma_desc->cq_mode = GNI_CQMODE_LOCAL_EVENT;
    fma_desc->src_cq_hndl = ugni_ctx.local_cq_handles[cqind];
  }

  fma_desc->type = opcode;
  //fma_desc->dlvr_mode = GNI_DLVMODE_PERFORMANCE;
  fma_desc->dlvr_mode = GNI_DLVMODE_IN_ORDER;
  fma_desc->local_addr = args->laddr;
  fma_desc->local_mem_hndl = args->lmdh;
  fma_desc->remote_addr = args->raddr;
  fma_desc->remote_mem_hndl = args->rmdh;
  fma_desc->length = args->size;
  fma_desc->post_id = args->id;
  fma_desc->rdma_mode = 0;

  sync_tatas_acquire(&cq_lock);
  {

    if (ugni_ctx.use_rcq && (flags & RDMA_FLAG_WITH_IMM)) {
      err = GNI_EpSetEventData(ugni_ctx.ep_handles[args->proc],
       			       (uint32_t)0x0,
       			       (uint32_t)args->imm_data);
      if (err != GNI_RC_SUCCESS) {
       	log_err("Could not set immediate RCQ event data");
       	goto error_exit;
      }
      fma_desc->cq_mode |= GNI_CQMODE_REMOTE_EVENT;
    }

    do {
      err = GNI_PostRdma(ugni_ctx.ep_handles[args->proc], fma_desc);
      if (err == GNI_RC_SUCCESS) {
	dbg_trace("GNI_PostRdma data transfer successful: %"PRIx64, args->id);
	break;
      }
      if (err != GNI_RC_ERROR_RESOURCE) {
	log_err("GNI_PostRdma data ERROR status: %s (%d)\n", gni_err_str[err], err);
	sync_tatas_release(&cq_lock);
	goto error_exit;
      }
      sched_yield();
    } while (++trials < max_trials);
    
    if (err == GNI_RC_ERROR_RESOURCE) {
      log_err("GNI_PostFma retries exceeded: %s (%d)", gni_err_str[err], err);
      goto error_exit;
    }
  }
  sync_tatas_release(&cq_lock);

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int __ugni_do_fma(struct rdma_args_t *args, int opcode, int flags) {
  gni_post_descriptor_t *fma_desc;
  int err, curr, curr_ind, cqind;
  const int max_trials = 1000;
  int trials = 0;

  cqind = PHOTON_GET_CQ_IND(ugni_ctx.num_cq, args->proc);  
  curr = sync_fadd(&descriptors[args->proc].curr, 1, SYNC_RELAXED);
  curr_ind = curr & (MAX_CQ_ENTRIES - 1);
  fma_desc = &(descriptors[args->proc].entries[curr_ind]);

  if (flags & RDMA_FLAG_NO_CQE) {
    fma_desc->cq_mode = GNI_CQMODE_SILENT;
    fma_desc->src_cq_hndl = NULL;
  }
  else {
    fma_desc->cq_mode = GNI_CQMODE_GLOBAL_EVENT;
    fma_desc->src_cq_hndl = ugni_ctx.local_cq_handles[cqind];
  }

  fma_desc->type = opcode;
  //fma_desc->dlvr_mode = GNI_DLVMODE_PERFORMANCE;
  fma_desc->dlvr_mode = GNI_DLVMODE_IN_ORDER;
  fma_desc->local_addr = args->laddr;
  fma_desc->local_mem_hndl = args->lmdh;
  fma_desc->remote_addr = args->raddr;
  fma_desc->remote_mem_hndl = args->rmdh;
  fma_desc->length = args->size;
  fma_desc->post_id = args->id;
  fma_desc->rdma_mode = 0;

  sync_tatas_acquire(&cq_lock);
  {
    if (ugni_ctx.use_rcq && (flags & RDMA_FLAG_WITH_IMM)) {
      err = GNI_EpSetEventData(ugni_ctx.ep_handles[args->proc],
       			       (uint32_t)0x0,
			       (uint32_t)args->imm_data);
      if (err != GNI_RC_SUCCESS) {
       	log_err("Could not set immediate RCQ event data");
	goto error_exit;
      }
      fma_desc->cq_mode |= GNI_CQMODE_REMOTE_EVENT;
    }

    do {
      err = GNI_PostFma(ugni_ctx.ep_handles[args->proc], fma_desc);
      if (err == GNI_RC_SUCCESS) {
	dbg_trace("GNI_PostFma data transfer successful: %"PRIx64, args->id);
	break;
      }
      if (err != GNI_RC_ERROR_RESOURCE) {
	log_err("GNI_PostFma data ERROR status: %s (%d)", gni_err_str[err], err);
	goto error_exit;
      }
      sched_yield();
    } while (++trials < max_trials);
    
    if (err == GNI_RC_ERROR_RESOURCE) {
      log_err("GNI_PostFma retries exceeded: %s (%d)", gni_err_str[err], err);
      goto error_exit;
    }
  }
  sync_tatas_release(&cq_lock);

  return PHOTON_OK;

error_exit:
  sync_tatas_release(&cq_lock);
  return PHOTON_ERROR;
}

static int ugni_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                         photonBuffer lbuf, photonBuffer rbuf, uint64_t id,
			 uint64_t imm, int flags) {
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
  args.imm_data = (uint32_t)imm;

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
                          photonBuffer lbuf, uint64_t id, uint64_t imm, int flags) {
  return PHOTON_OK;
}

static int ugni_rdma_recv(photonAddr addr, uintptr_t laddr, uint64_t size,
                          photonBuffer lbuf, uint64_t id, int flags) {
  return PHOTON_OK;
}

static int ugni_get_event(int proc, int max, photon_rid *ids, int *n) {
  gni_post_descriptor_t *event_post_desc_ptr;
  gni_cq_entry_t current_event;
  uint64_t cookie = NULL_REQUEST;
  int i, rc, start, end, comp;

  *n = 0;
  comp = 0;

  if (ugni_ctx.num_cq == 1) {
    start = 0;
    end = 1;
  }
  else if (proc == PHOTON_ANY_SOURCE) {
    start = 0;
    end = ugni_ctx.num_cq;
  }
  else {
    start = PHOTON_GET_CQ_IND(ugni_ctx.num_cq, proc);
    end = start+1;
  }
  
  sync_tatas_acquire(&cq_lock);
  for (i=start; i<end && comp<max; i++) {
    rc = get_cq_event(ugni_ctx.local_cq_handles[i], 1, 0, &current_event);
    if (rc == 0) {
      rc = GNI_GetCompleted(ugni_ctx.local_cq_handles[i], current_event, &event_post_desc_ptr);
      cookie = event_post_desc_ptr->post_id;
      if (rc != GNI_RC_SUCCESS) {
	dbg_err("GNI_GetCompleted data ERROR status: %s (%d)", gni_err_str[rc], rc);
      }
    }
    else if (rc == 3) {
      // nothing available
      continue;
    }
    else {
      // rc == 2 is an overrun
      dbg_err("Error getting CQ event: %d", rc);
      goto error_exit;
    }
    
    dbg_trace("received event with cookie:%"PRIx64, cookie);
    ids[comp] = cookie;
    comp++;
  }
  sync_tatas_release(&cq_lock);  

  *n = comp;
  
  if (comp == 0) {
    return PHOTON_EVENT_NONE;
  }
  
  return PHOTON_EVENT_OK;
  
 error_exit:
  sync_tatas_release(&cq_lock);
  return PHOTON_EVENT_ERROR;
}

static int ugni_get_revent(int proc, int max, photon_rid *ids, uint64_t *imms, int *n) {
  gni_cq_entry_t current_event;
  uint64_t cookie = NULL_REQUEST;
  uint64_t imm_data;
  int rc, comp;

  if (!ugni_ctx.use_rcq) {
    return PHOTON_EVENT_NOTIMPL;
  }
  
  *n = 0;
  comp = 0;

  sync_tatas_acquire(&cq_lock);
  do {
    rc = get_cq_event(ugni_ctx.remote_cq_handles[0], 1, 0, &current_event);
    if (rc == 0) {
      imm_data = GNI_CQ_GET_REM_INST_ID(current_event);
      ids[comp] = cookie;
      imms[comp] = imm_data;
      comp++;
    }
    else if (rc == 3) {
      // nothing available
      break;
    }
    else {
      // rc == 2 is an overrun
      dbg_err("Error getting CQ event: %d", rc);
      goto error_exit;
    }
  } while (comp < max);
  sync_tatas_release(&cq_lock);  
  
  *n = comp;
  
  if (comp == 0) {
    return PHOTON_EVENT_NONE;
  }
  
  return PHOTON_EVENT_OK;
  
 error_exit:
  sync_tatas_release(&cq_lock);
  return PHOTON_EVENT_ERROR;
}

