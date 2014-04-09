#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "photon_backend.h"
#include "photon_buffer.h"

#include "verbs.h"
#include "verbs_connect.h"
#include "verbs_exchange.h"
#include "verbs_ud.h"
#include "logging.h"

#define MAX_RETRIES 1

struct rdma_args_t {
  int proc;
  uint64_t id;
  struct ibv_sge *sg_list;
  int num_sge;
  uintptr_t raddr;
  uint32_t rkey;
};

struct sr_args_t {
  int proc;
  uint64_t id;
  struct ibv_sge *sg_list;
  int num_sge;
  struct ibv_ah *ah;
  uint32_t qpn;
  uint32_t qkey;
};

static int __initialized = 0;

static int verbs_initialized(void);
static int verbs_init(photonConfig cfg, ProcessInfo *photon_processes, photonBI ss);
static int verbs_finalize(void);
static int verbs_connect_single(void *local_ci, void *remote_ci, int pindex, void **ret_ci, int *ret_len, photon_connect_mode_t mode);
static int verbs_get_info(ProcessInfo *pi, int proc, void **info, int *size, photon_info_t type);
static int verbs_set_info(ProcessInfo *pi, int proc, void *info, int size, photon_info_t type);
static int verbs_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                          photonBuffer lbuf, photonBuffer rbuf, uint64_t id);
static int verbs_rdma_get(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                          photonBuffer lbuf, photonBuffer rbuf, uint64_t id);
static int verbs_rdma_send(photonAddr addr, uintptr_t laddr, uint64_t size,
                           photonBuffer lbuf, uint64_t id);
static int verbs_rdma_recv(photonAddr addr, uintptr_t laddr, uint64_t size,
                           photonBuffer lbuf, uint64_t id);
static int verbs_get_event(photonEventStatus stat);
static int verbs_register_addr(photonAddr addr, int af);
static int verbs_unregister_addr(photonAddr addr, int af);

static int __verbs_do_rdma(struct rdma_args_t *args, int opcode);
static int __verbs_do_send(struct sr_args_t *args);
static int __verbs_do_recv(struct sr_args_t *args);

static verbs_cnct_ctx verbs_ctx = {
  .ib_dev ="ib0",
  .ib_port = 1,
  .ib_context = NULL,
  .ib_pd = NULL,
  .ib_cq = NULL,
  .ib_srq = NULL,
  .ib_cc = NULL,
  .ib_lid = 0,
  .cm_schannel = NULL,
  .cm_id = NULL,
  .qp = NULL,
  .ud_qp = NULL,
  .psn = 0,
  .num_qp = 0,
  .use_ud = 0,
  .tx_depth = LEDGER_SIZE,
  .rx_depth = LEDGER_SIZE,
  .atomic_depth = 16,
  .max_sge = 16
};

/* we are now a Photon backend */
struct photon_backend_t photon_verbs_backend = {
  .context = &verbs_ctx,
  .initialized = verbs_initialized,
  .init = verbs_init,
  .finalize = verbs_finalize,
  .connect = verbs_connect_single,
  .get_info = verbs_get_info,
  .set_info = verbs_set_info,
  /* API */
  .register_addr = verbs_register_addr,
  .unregister_addr = verbs_unregister_addr,
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
  .rdma_put = verbs_rdma_put,
  .rdma_get = verbs_rdma_get,
  .rdma_send = verbs_rdma_send,
  .rdma_recv = verbs_rdma_recv,
  .get_event = verbs_get_event
};

static int verbs_initialized() {
  if (__initialized == 1)
    return PHOTON_OK;
  else
    return PHOTON_ERROR_NOINIT;
}

static int verbs_init(photonConfig cfg, ProcessInfo *photon_processes, photonBI ss) {

  /* __initialized: 0 - not; -1 - initializing; 1 - initialized */
  __initialized = -1;

  if (cfg->ib_dev)
    verbs_ctx.ib_dev = cfg->ib_dev;

  if (cfg->ib_port)
    verbs_ctx.ib_port = cfg->ib_port;

  if (cfg->use_cma && !cfg->eth_dev) {
    log_err("CMA specified but Ethernet dev missing");
    goto error_exit;
  }

  if (cfg->use_ud) {
    verbs_ctx.use_ud = cfg->use_ud;
  }

  if(__verbs_init_context(&verbs_ctx)) {
    log_err("Could not initialize verbs context");
    goto error_exit;
  }

  /* a forwarder connects to itself so we get a protection domain */
  if (_forwarder) {
    __verbs_connect_single(&verbs_ctx, verbs_ctx.local_ci[_photon_myrank],
                           verbs_ctx.local_ci[_photon_myrank], _photon_myrank,
                           NULL, NULL,
                           PHOTON_CONN_ACTIVE);
  }
  else {
    if(__verbs_connect_peers(&verbs_ctx)) {
      log_err("Could not connect peers");
      goto error_exit;
    }

    /* setup forwarder if requested */
    if (cfg->use_forwarder && __photon_forwarder) {
      if (__photon_forwarder->init(cfg, photon_processes)) {
        log_err("Could not initialize forwarder(s)");
        goto error_exit;
      }

      if (__photon_forwarder->connect(photon_processes)) {
        log_err("Could not perform connect with forwarder(s)");
        goto error_exit;
      }
    }
  }

  /* this shared buffer needs to be registered before any exchanges
     since we share a common rkey across ledgers
     and to register, we need a protection domain, hence at least one
     connect must be made first (CMA case) */
  if (photon_buffer_register(ss, &verbs_ctx) != 0) {
    log_err("couldn't register local buffer for the ledger entries");
    goto error_exit;
  }

  if (!_forwarder) {
    /* TODO: pull out exchange and generalize in libphoton */
    if (cfg->use_forwarder && __photon_forwarder) {
      if (__photon_forwarder->exchange(photon_processes)) {
        log_err("Could not perform ledger exchange with forwarder(s)");
        goto error_exit;
      }
    }

    if (__verbs_exchange_ri_ledgers(photon_processes) != 0) {
      log_err("couldn't exchange rdma ledgers");
      goto error_exit;
    }

    if (__verbs_exchange_FIN_ledger(photon_processes) != 0) {
      log_err("couldn't exchange send ledgers");
      goto error_exit;
    }
  }

  __initialized = 1;

  dbg_info("ended successfully =============");

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int verbs_finalize() {
  /* should clean up allocated buffers */
  return PHOTON_OK;
}

static int verbs_connect_single(void *local_ci, void *remote_ci, int pindex, void **ret_ci, int *ret_len, photon_connect_mode_t mode) {
  return __verbs_connect_single(&verbs_ctx, local_ci, remote_ci, pindex, (verbs_cnct_info**)ret_ci, ret_len, mode);
}

static int verbs_get_info(ProcessInfo *pi, int proc, void **ret_info, int *ret_size, photon_info_t type) {
  int i;
  struct photon_buffer_t *info;
  extern photonBI shared_storage;

  switch (type) {
  case PHOTON_MTU:
    {
      *ret_info = &verbs_ctx.ib_mtu;
      *ret_size = sizeof(verbs_ctx.ib_mtu);
    }
    break;
  case PHOTON_CI: {
    verbs_cnct_info *cinfo;
    cinfo = (verbs_cnct_info *)malloc(MAX_QP * sizeof(verbs_cnct_info));
    if(!cinfo) {
      goto error_exit;
    }
    for (i=0; i<MAX_QP; i++) {
      memcpy(&cinfo[i], verbs_ctx.local_ci[proc], sizeof(verbs_cnct_info));
    }
    *ret_info = (void*)cinfo;
    *ret_size = MAX_QP * sizeof(*cinfo);
  }
  break;
  case PHOTON_RI:
  case PHOTON_SI:
  case PHOTON_FI: {
    info = (struct photon_buffer_t *)malloc(sizeof(struct photon_buffer_t));
    if (!info) {
      goto error_exit;
    }
    info->priv = shared_storage->buf.priv;
    *ret_info = (void*)info;
    *ret_size = sizeof(*info);
  }
  break;
  default:
    goto error_exit;
    break;
  }

  switch (type) {
  case PHOTON_RI:
    info->addr = (uintptr_t)pi->local_rcv_info_ledger->entries;
    break;
  case PHOTON_SI:
    info->addr = (uintptr_t)pi->local_snd_info_ledger->entries;
    break;
  case PHOTON_FI:
    info->addr = (uintptr_t)pi->local_FIN_ledger->entries;
  default:
    break;
  }

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int verbs_set_info(ProcessInfo *pi, int proc, void *info, int size, photon_info_t type) {
  photonBuffer li = (photonBuffer)info;

  switch (type) {
  case PHOTON_CI: {
    int i;
    for (i=0; i<MAX_QP; i++) {
      memcpy(&verbs_ctx.remote_ci[proc][i], info, sizeof(verbs_cnct_info));
    }
  }
  break;
  case PHOTON_RI:
    pi->remote_rcv_info_ledger->remote.addr = li->addr;
    pi->remote_rcv_info_ledger->remote.priv = li->priv;
    break;
  case PHOTON_SI:
    pi->remote_snd_info_ledger->remote.addr = li->addr;
    pi->remote_snd_info_ledger->remote.priv = li->priv;
    break;
  case PHOTON_FI:
    pi->remote_FIN_ledger->remote.addr = li->addr;
    pi->remote_FIN_ledger->remote.priv = li->priv;
    break;
  default:
    goto error_exit;
    break;
  }

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int __verbs_do_rdma(struct rdma_args_t *args, int opcode) {
  int err, retries;
  struct ibv_send_wr *bad_wr;

  struct ibv_send_wr wr = {
    .wr_id               = args->id,
    .sg_list             = args->sg_list,
    .num_sge             = args->num_sge,
    .opcode              = opcode,
    .send_flags          = IBV_SEND_SIGNALED,
    .wr.rdma.remote_addr = args->raddr,
    .wr.rdma.rkey        = args->rkey
  };

  retries = MAX_RETRIES;
  do {
    err = ibv_post_send(verbs_ctx.qp[args->proc], &wr, &bad_wr);
  }
  while(err && --retries);

  if (err != 0) {
    dbg_err("Failure in ibv_post_send(): %s", strerror(err));
    return PHOTON_ERROR;
  }

  return PHOTON_OK;
}

// send/recv use UD service_qp by default at the moment
static int __verbs_do_send(struct sr_args_t *args) {
  int err, retries;
  struct ibv_send_wr *bad_wr;

  struct ibv_send_wr wr = {
    .wr_id               = args->id,
    .sg_list             = args->sg_list,
    .num_sge             = args->num_sge,
    .opcode              = IBV_WR_SEND,
    .send_flags          = IBV_SEND_SIGNALED,
    .wr = {
      .ud = {
        .ah              = args->ah,
        .remote_qpn      = args->qpn,
        .remote_qkey     = args->qkey
      }
    }
  };
  
  retries = MAX_RETRIES;
  do {
    err = ibv_post_send(verbs_ctx.ud_qp, &wr, &bad_wr);
  }
  while(err && --retries);

  if (err != 0) {
    dbg_err("Failure in ibv_post_send(): %s", strerror(err));
    return PHOTON_ERROR;
  }

  return PHOTON_OK;
}

static int __verbs_do_recv(struct sr_args_t *args) {
  int err, retries;
  struct ibv_recv_wr *bad_wr;

  struct ibv_recv_wr wr = {
    .wr_id               = args->id,
    .sg_list             = args->sg_list,
    .num_sge             = args->num_sge,
    .next                = NULL
  };

  retries = MAX_RETRIES;
  do {
    err = ibv_post_recv(verbs_ctx.ud_qp, &wr, &bad_wr);
  }
  while(err && --retries);

  if (err != 0) {
    dbg_err("Failure in ibv_post_recv(): (%d) %s", err, strerror(err));
    return PHOTON_ERROR;
  }

  return PHOTON_OK;
}

static int verbs_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                          photonBuffer lbuf, photonBuffer rbuf, uint64_t id) {
  struct rdma_args_t args;
  struct ibv_sge list = {
    .addr   = laddr,
    .length = size,
    .lkey   = lbuf->priv.key0
  };
  
  args.proc = proc;
  args.id = id;
  args.sg_list = &list;
  args.num_sge = 1;
  args.raddr = raddr;
  args.rkey = rbuf->priv.key1;
  return __verbs_do_rdma(&args, IBV_WR_RDMA_WRITE);
}

static int verbs_rdma_get(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                          photonBuffer lbuf, photonBuffer rbuf, uint64_t id) {
  struct rdma_args_t args;
  struct ibv_sge list = {
    .addr   = laddr,
    .length = size,
    .lkey   = lbuf->priv.key0
  };

  args.proc = proc;
  args.id = id;
  args.sg_list = &list;
  args.num_sge = 1;
  args.raddr = raddr;
  args.rkey = rbuf->priv.key1;
  return __verbs_do_rdma(&args, IBV_WR_RDMA_READ);
}

static int verbs_rdma_send(photonAddr addr, uintptr_t laddr, uint64_t size,
                           photonBuffer lbuf, uint64_t id) {
  struct sr_args_t args;
  struct ibv_sge list = {
    .addr = laddr,
    .length = size,
    .lkey = lbuf->priv.key0
  };

  __verbs_ud_create_ah(&verbs_ctx, (union ibv_gid *)addr, 0x0, &args.ah);

  args.proc = addr->global.proc_id;
  args.id = id;
  args.sg_list = &list;
  args.num_sge = 1;
  args.qpn = 0xffffff;
  args.qkey = 0x11111111;
  return __verbs_do_send(&args);
}

// 32 least-significant bits of id should index the mbuf entries
static int verbs_rdma_recv(photonAddr addr, uintptr_t laddr, uint64_t size,
                           photonBuffer lbuf, uint64_t id) {
  struct sr_args_t args;
  struct ibv_sge list = {
    .addr = laddr,
    .length = size,
    .lkey = lbuf->priv.key0
  };
  
  if (addr)
    args.proc = addr->global.proc_id;
  args.id = id;
  args.sg_list = &list;
  args.num_sge = 1;
  return __verbs_do_recv(&args);
}

static int verbs_get_event(photonEventStatus stat) {
  int ne;
  struct ibv_wc wc;

  if (!stat) {
    log_err("NULL status pointer");
    goto error_exit;
  }

  do {
    ne = ibv_poll_cq(verbs_ctx.ib_cq, 1, &wc);
    if (ne < 0) {
      log_err("ibv_poll_cq() failed");
      goto error_exit;
    }
  }
  while (ne < 1);

  if (wc.status != IBV_WC_SUCCESS) {
    log_err("(status==%d) != IBV_WC_SUCCESS: %s", wc.status, strerror(wc.status));
    goto error_exit;
  }
  
  stat->id = wc.wr_id;
  stat->proc = 0x0;
  stat->priv = NULL;

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int verbs_register_addr(photonAddr addr, int af) {
  if (verbs_ctx.use_ud) {
    return __verbs_ud_attach_addr(&verbs_ctx, (union ibv_gid*)addr);
  }
  else {
    dbg_warn("Unknown action");
  }
  
  return PHOTON_OK;
}

static int verbs_unregister_addr(photonAddr addr, int af) {
  if (verbs_ctx.use_ud) {
    return __verbs_ud_detach_addr(&verbs_ctx, (union ibv_gid*)addr);
  }
  else {
    dbg_warn("Unknown action");
  }

  return PHOTON_OK;
}
