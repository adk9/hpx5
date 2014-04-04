#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>

#include "libphoton.h"
#include "logging.h"
#include "verbs_connect.h"
#include "verbs_exchange.h"
#include "verbs_ud.h"

#define MAX_CQ_ENTRIES      1000
#define RDMA_CMA_BASE_PORT  18000
#define RDMA_CMA_FORW_OFFS  1000
#define RDMA_CMA_TIMEOUT    2000
#define RDMA_CMA_RETRIES    10

static pthread_t cma_listener;

static verbs_cnct_info **__exch_cnct_info(verbs_cnct_ctx *ctx, verbs_cnct_info **local_info, int num_qp);
static int __verbs_connect_qps_cma(verbs_cnct_ctx *ctx, verbs_cnct_info *local_info, verbs_cnct_info *remote_info, int pindex, int num_qp);
static int __verbs_connect_qps(verbs_cnct_ctx *ctx, verbs_cnct_info *local_info, verbs_cnct_info *remote_info, int pindex, int num_qp);
static int __verbs_init_context_cma(verbs_cnct_ctx *ctx, struct rdma_cm_id *cm_id);
static int __verbs_create_connect_info(verbs_cnct_ctx *ctx);
static void *__rdma_cma_listener_thread(void *arg);

// data that gets exchanged during RDMA CMA connect
// for now we simply identify remote peers by address (rank)
struct rdma_cma_priv {
  uint64_t address;
};

struct rdma_cma_thread_args {
  verbs_cnct_ctx *ctx;
  int pindex;
  int num_listeners;
};

int __verbs_init_context(verbs_cnct_ctx *ctx) {
  struct ibv_device **dev_list;
  struct ibv_context **ctx_list;
  int i, iproc, num_devs;

  // initialize the QP array
  ctx->num_qp = MAX_QP;
  ctx->qp = (struct ibv_qp**)malloc((_photon_nproc + _photon_nforw) * sizeof(struct ibv_qp*));
  if (!ctx->qp) {
    log_err("Could not allocate QP memory");
    return PHOTON_ERROR;
  }
  // initialize the QP array that gets set later
  for (i=0; i<(_photon_nproc + _photon_nforw); i++) {
    ctx->qp[i] = NULL;
  }

  if (__photon_config->use_cma) {

    ctx->cm_id = (struct rdma_cm_id**)malloc((_photon_nproc + _photon_nforw) * sizeof(struct rdma_cm_id*));
    if (!ctx->cm_id) {
      log_err("Could not allocate CM_ID memory");
      return PHOTON_ERROR;
    }
    // initialize the CM_ID array that gets set later
    for (i=0; i<(_photon_nproc + _photon_nforw); i++) {
      ctx->qp[i] = NULL;
    }

    ctx->cm_schannel = rdma_create_event_channel();
    if (!ctx->cm_schannel) {
      dbg_err("Could not create RDMA event channel");
      return PHOTON_ERROR;
    }

    // make sure there are some devices we can use
    ctx_list = rdma_get_devices(&num_devs);
    if (!num_devs) {
      dbg_err("No RDMA CMA devices found");
      return PHOTON_ERROR;
    }

    // start the RDMA CMA listener
    // first rank only connects
    // forwarders will listen for a self-connection to start
    struct rdma_cma_thread_args *args;
    if (!_forwarder && (_photon_myrank > 0)) {
      args = malloc(sizeof(struct rdma_cma_thread_args));
      args->ctx = ctx;
      args->pindex = -1;
      args->num_listeners = _photon_myrank;
      pthread_create(&cma_listener, NULL, __rdma_cma_listener_thread, (void*)args);
    }
    else if (_forwarder) {
      args = malloc(sizeof(struct rdma_cma_thread_args));
      args->ctx = ctx;
      args->pindex = -1;
      args->num_listeners = 1;
      pthread_create(&cma_listener, NULL, __rdma_cma_listener_thread, (void*)args);
    }
  }
  else {
    dev_list = ibv_get_device_list(&num_devs);
    if (!num_devs) {
      dbg_err("No IB devices found");
      return PHOTON_ERROR;
    }

    for (i=0; i<num_devs; i++) {
      if (!strcmp(ibv_get_device_name(dev_list[i]), ctx->ib_dev)) {
        dbg_info("using device %s:%d", ibv_get_device_name(dev_list[i]), ctx->ib_port);
        break;
      }
    }

    if (i==num_devs) {
      log_err("Could not find IB device: %s", ctx->ib_dev);
      return PHOTON_ERROR;
    }

    ctx->ib_context = ibv_open_device(dev_list[i]);
    if (!ctx->ib_context) {
      dbg_err("Could not get context for %s\n", ibv_get_device_name(dev_list[i]));
      return PHOTON_ERROR;
    }
    dbg_info("context has device %s", ibv_get_device_name(ctx->ib_context->device));

    // get my local lid
    struct ibv_port_attr attr;
    memset(&attr, 0, sizeof(attr));
    if(ibv_query_port(ctx->ib_context, ctx->ib_port, &attr) ) {
      dbg_err("Could not query port");
      return PHOTON_ERROR;
    }
    ctx->ib_lid = attr.lid;
    ctx->ib_mtu = 1 << (attr.active_mtu + 7);

    ctx->ib_pd = ibv_alloc_pd(ctx->ib_context);
    if (!ctx->ib_pd) {
      dbg_err("Could not create protection domain");
      return PHOTON_ERROR;
    }

    // create a completion queue
    // The second argument (cq_size) can be something like 40K.	 It should be
    // within NIC MaxCQEntries limit
    ctx->ib_cq = ibv_create_cq(ctx->ib_context, MAX_CQ_ENTRIES, ctx, NULL,  0);
    if (!ctx->ib_cq) {
      dbg_err("Could not create completion queue");
      return PHOTON_ERROR;
    }

    {
      // create shared receive queue
      struct ibv_srq_init_attr attr = {
        .attr = {
          .max_wr  = 500,
          .max_sge = 1
        }
      };

      ctx->ib_srq = ibv_create_srq(ctx->ib_pd, &attr);
      if (!ctx->ib_srq) {
        dbg_err("Could not create SRQ");
        return PHOTON_ERROR;
      }
    }

    // create QPs in the non-CMA case and transition to INIT state
    // RDMA CMA does this transition for us when we connect
    for (iproc = 0; iproc < (_photon_nproc + _photon_nforw); ++iproc) {
      
      // only one QP supported
      ctx->qp[iproc] = (struct ibv_qp*)malloc(sizeof(struct ibv_qp));
      if (!ctx->qp[iproc]) {
        log_err("Could not allocated space for new QP");
        return PHOTON_ERROR;
      }

      struct ibv_qp_init_attr attr = {
        .qp_context     = ctx,
        .send_cq        = ctx->ib_cq,
        .recv_cq        = ctx->ib_cq,
        .srq            = ctx->ib_srq,
        .cap            = {
          .max_send_wr	   = ctx->tx_depth,
          .max_send_sge    = 1, // scatter gather element
          .max_recv_wr     = ctx->rx_depth,
          .max_recv_sge    = 1, // scatter gather element
          .max_inline_data = 0
        },
        .qp_type        = IBV_QPT_RC
      };

      ctx->qp[iproc] = ibv_create_qp(ctx->ib_pd, &attr);
      if (!(ctx->qp[iproc])) {
        dbg_err("Could not create QP for task:%d", iproc);
        return PHOTON_ERROR;
      }

      {
        struct ibv_qp_attr attr;

        attr.qp_state    = IBV_QPS_INIT;
        attr.pkey_index	 = 0;
        attr.port_num	 = ctx->ib_port;
        attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;

        if (ibv_modify_qp(ctx->qp[iproc], &attr,
                          IBV_QP_STATE		 |
                          IBV_QP_PKEY_INDEX	 |
                          IBV_QP_PORT		 |
                          IBV_QP_ACCESS_FLAGS)) {
          dbg_err("Failed to modify QP for task:%d to INIT: %s", iproc, strerror(errno));
          return PHOTON_ERROR;
        }
      }
    }
    // create a UD QP as well if requested
    if (ctx->use_ud) {
      __verbs_ud_create_qp(ctx);
    }
  }
  
  // init context also creates connect info for all procs
  return __verbs_create_connect_info(ctx);
}

// we make connect info for all procs, including any forwarders
int __verbs_create_connect_info(verbs_cnct_ctx *ctx) {
  struct ifaddrs *ifaddr, *ifa;
  int i, j, iproc;

  ctx->local_ci = (verbs_cnct_info **)malloc((_photon_nproc + _photon_nforw) * sizeof(verbs_cnct_info *) );
  if( !ctx->local_ci ) {
    goto error_exit;
  }

  if (getifaddrs(&ifaddr) == -1) {
    dbg_err("verbs_connect_peers(): Cannot get interface addrs");
    goto error_exit;
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL)
      continue;

    if (!strcmp(ifa->ifa_name, __photon_config->eth_dev) &&
        ifa->ifa_addr->sa_family == AF_INET) {
      break;
    }
  }

  if (__photon_config->use_cma && !ifa) {
    dbg_err("verbs_connect_peers(): Did not find interface info for %s\n", __photon_config->eth_dev);
    goto error_exit;
  }

  for(iproc=0; iproc < (_photon_nproc + _photon_nforw); ++iproc) {

    ctx->local_ci[iproc] = (verbs_cnct_info *)malloc( MAX_QP*sizeof(verbs_cnct_info) );
    if( !ctx->local_ci[iproc] ) {
      goto error_exit;
    }

    for(i=0; i<MAX_QP; ++i) {
      
      memset(&(ctx->local_ci[iproc][i].gid.raw), 0, sizeof(union ibv_gid));
      
      if (__photon_config->use_cma) {
        ctx->local_ci[iproc][i].qpn = 0x0;
      }
      else {
        // can only query gid in in non-CMA mode, CMA will exchange this for us
        if (ibv_query_gid(ctx->ib_context, ctx->ib_port, 0, &(ctx->local_ci[iproc][i].gid))) {
          dbg_info("Could not get local gid for gid index 0");
        }
        ctx->local_ci[iproc][i].qpn = ctx->qp[iproc]->qp_num;
      }

      if (ifa) {
        ctx->local_ci[iproc][i].ip = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
        ctx->local_ci[iproc][i].cma_port = RDMA_CMA_BASE_PORT + _photon_myrank;
      }

      ctx->local_ci[iproc][i].lid = ctx->ib_lid;
      ctx->local_ci[iproc][i].psn = (lrand48() & 0xfff000) + _photon_myrank+1;
    }
  }

  /* also allocate space for the remote info */
  ctx->remote_ci = (verbs_cnct_info **)malloc((_photon_nproc + _photon_nforw) * sizeof(verbs_cnct_info *) );
  for (iproc = 0; iproc < (_photon_nproc + _photon_nforw); ++iproc) {

    ctx->remote_ci[iproc] = (verbs_cnct_info *)malloc(MAX_QP * sizeof(verbs_cnct_info));
    if (!ctx->remote_ci) {
      for (j = 0; j < iproc; j++) {
        free(ctx->remote_ci[j]);
      }
      free(ctx->remote_ci);
      goto error_exit;
    }
  }
  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

int __verbs_connect_single(verbs_cnct_ctx *ctx, verbs_cnct_info *local_info, verbs_cnct_info *remote_info, int pindex,
                           verbs_cnct_info **ret_ci, int *ret_len, photon_connect_mode_t mode) {
  switch (mode) {
  case PHOTON_CONN_ACTIVE:
    if (__photon_config->use_cma) {
      return __verbs_connect_qps_cma(ctx, local_info, remote_info, pindex, MAX_QP);
    }
    else {
      return __verbs_connect_qps(ctx, local_info, remote_info, pindex, MAX_QP);
    }
    break;
  case PHOTON_CONN_PASSIVE:
    if (__photon_config->use_cma) {
      pthread_t cma_thread;
      struct rdma_cma_thread_args *args;
      args = malloc(sizeof(struct rdma_cma_thread_args));
      args->ctx = ctx;
      args->pindex = pindex;
      args->num_listeners = 1;
      pthread_create(&cma_thread, NULL, __rdma_cma_listener_thread, (void*)args);

      /* if we are some external forwarder, return which port we're listening on for the peer */
      if (_forwarder && ret_ci && ret_len) {
        *ret_ci = malloc(sizeof(verbs_cnct_info));
        memcpy(*ret_ci, local_info, sizeof(verbs_cnct_info));
        (*ret_ci)->cma_port = RDMA_CMA_BASE_PORT + RDMA_CMA_FORW_OFFS + pindex;
        *ret_len = sizeof(verbs_cnct_info);
      }
      return PHOTON_OK;
    }
    else {
      return __verbs_connect_qps(ctx, local_info, remote_info, pindex, MAX_QP);
    }
  default:
    return PHOTON_ERROR;
  }
}

int __verbs_connect_peers(verbs_cnct_ctx *ctx) {
  int iproc;
  MPI_Comm _photon_comm = __photon_config->comm;

  dbg_info();

  ctx->remote_ci = __exch_cnct_info(ctx, ctx->local_ci, MAX_QP);
  if( !ctx->remote_ci ) {
    dbg_err("Cannot exchange connect info");
    goto error_exit;
  }

  MPI_Barrier(_photon_comm);

  if (__photon_config->use_cma) {
    // in the CMA case, only connect actively for ranks greater than or equal to our rank
    for (iproc = (_photon_myrank + 1); iproc < _photon_nproc; iproc++) {
      if (iproc == _photon_myrank) {
        continue;
      }

      if (__verbs_connect_qps_cma(ctx, ctx->local_ci[iproc], ctx->remote_ci[iproc], iproc, MAX_QP)) {
        dbg_err("Could not connect queue pairs using RDMA CMA");
        goto error_exit;
      }
    }
  }
  else {
    for (iproc = 0; iproc < _photon_nproc; iproc++) {
      if( iproc == _photon_myrank ) {
        continue;
      }

      if(__verbs_connect_qps(ctx, ctx->local_ci[iproc], ctx->remote_ci[iproc], iproc, MAX_QP)) {
        dbg_err("Cannot connect queue pairs");
        goto error_exit;
      }
    }
  }

  // make sure everyone is connected before proceeding
  if (__photon_config->use_cma) {
    if (_photon_myrank > 0) {
      dbg_info("waiting for listener to finish...");
      pthread_join(cma_listener, NULL);
      dbg_info("DONE");
    }

    MPI_Barrier(_photon_comm);
  }

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int __verbs_init_context_cma(verbs_cnct_ctx *ctx, struct rdma_cm_id *cm_id) {
  // assign the verbs context if this is the first connection
  if (!ctx->ib_context) {
    ctx->ib_context = cm_id->verbs;
    if (!ctx->ib_context) {
      dbg_err("could not get context from CM ID!");
      goto error_exit;
    }

    // create new PD with the new context
    ctx->ib_pd = ibv_alloc_pd(ctx->ib_context);
    if (!ctx->ib_pd) {
      dbg_err("could not create PD");
      goto error_exit;
    }

    ctx->ib_cc = ibv_create_comp_channel(ctx->ib_context);
    if (!ctx->ib_cc) {
      dbg_err("could not create completion channel");
      goto error_exit;
    }

    ctx->ib_cq = ibv_create_cq(ctx->ib_context, MAX_CQ_ENTRIES, ctx, ctx->ib_cc, 0);
    if (!ctx->ib_cq) {
      dbg_err("could not create CQ");
      goto error_exit;
    }

    if (ibv_req_notify_cq(ctx->ib_cq, 0)) {
      dbg_err("could not request CQ notifications");
      goto error_exit;
    }
    
    struct ibv_port_attr port_attr;
    if (ibv_query_port(cm_id->verbs, cm_id->port_num, &port_attr)) {
      dbg_err("could not query port");
      goto error_exit;
    }
    ctx->ib_lid = port_attr.lid;
    ctx->ib_mtu = 1 << (port_attr.active_mtu + 7);    

    // create a UD QP as well if requested
    if (ctx->use_ud) {
      __verbs_ud_create_qp(ctx);
    }
  }
  else {
    if (ctx->ib_context != cm_id->verbs) {
      dbg_err("CM ID has a new ib_context (different adapter), not supported!");
      goto error_exit;
    }
  }

  struct ibv_qp_init_attr attr = {
    .qp_context = ctx,
    .send_cq = ctx->ib_cq,
    .recv_cq = ctx->ib_cq,
    .cap     = {
      .max_send_wr  = ctx->tx_depth,
      .max_recv_wr  = ctx->rx_depth,
      .max_send_sge = 1,
      .max_recv_sge = 1,
      .max_inline_data = 0
    },
    .qp_type = IBV_QPT_RC,
    //.sq_sig_all = 0,
    .srq = NULL
  };

  // create a new QP for each connection
  if (rdma_create_qp(cm_id, ctx->ib_pd, &attr)) {
    dbg_err("could not create QP");
    goto error_exit;
  }

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static void *__rdma_cma_listener_thread(void *arg) {
  struct rdma_cma_thread_args *args = (struct rdma_cma_thread_args*)arg;
  verbs_cnct_ctx *ctx = args->ctx;
  int num_listeners = args->num_listeners;
  int n, num_connected = 0;
  int pindex;
  int port;
  char *service;

  struct addrinfo *res;
  struct addrinfo hints = {
    .ai_flags    = AI_PASSIVE,
    .ai_family   = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM
  };
  struct rdma_cm_event *event;
  struct sockaddr_in sin;
  struct rdma_cm_id *local_cm_id;
  struct rdma_cm_id *child_cm_id;
  struct rdma_conn_param conn_param;
  struct rdma_event_channel *echannel;

  if (args->pindex >= 0) {
    port = RDMA_CMA_BASE_PORT + RDMA_CMA_FORW_OFFS + args->pindex;
  }
  else {
    port = RDMA_CMA_BASE_PORT + _photon_myrank;
  }

  if (asprintf(&service, "%d", port) < 0)
    goto error_exit;

  if ((n = getaddrinfo(NULL, service, &hints, &res)) < 0) {
    dbg_err("%s for port %d\n", gai_strerror(n), port);
    goto error_exit;
  }

  sin.sin_addr.s_addr = 0;
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);

  echannel = rdma_create_event_channel();
  if (!echannel) {
    dbg_err("Could not create RDMA event channel");
    goto error_exit;
  }

  if (rdma_create_id(echannel, &local_cm_id, NULL, RDMA_PS_TCP)) {
    dbg_err("Could not create CM ID");
    goto error_exit;
  }

  if (rdma_bind_addr(local_cm_id, (struct sockaddr *)&sin)) {
    dbg_err("rdma_bind_addr failed: %s", strerror(errno));
    goto error_exit;
  }

  if (rdma_listen(local_cm_id, 0)) {
    dbg_err("rdma_listen failed: %s", strerror(errno));
    goto error_exit;
  }

  dbg_info("Listening for %d connections on port %d", num_listeners, port);

  // accept some number of connections
  // this currently depends on rank position
  do {

    if (rdma_get_cm_event(echannel, &event)) {
      dbg_err("coult not get event %s", strerror(errno));
      goto error_exit;
    }

    switch (event->event) {
    case RDMA_CM_EVENT_CONNECT_REQUEST: {
      child_cm_id = (struct rdma_cm_id *)event->id;
      if (!child_cm_id) {
        dbg_err("could not get child CM ID");
        goto error_exit;
      }

      // figure out who just connected to us and accept
      struct rdma_cma_priv *priv;
      if (event->param.conn.private_data &&
          (event->param.conn.private_data_len > 0)) {
        priv = (struct rdma_cma_priv*)event->param.conn.private_data;
        // no need to hold onto this data
        //ctx->remote_priv[priv->address] = malloc(event->param.conn.private_data_len);
        //memcpy(ctx->remote_priv[priv->address],
        //       event->param.conn.private_data,
        //       event->param.conn.private_data_len);
      }
      else {
        // TODO: use another mechanism to identify remote peer
        dbg_err("no remote connect info found");
        goto error_exit;
      }

      dbg_info("got connection request from %d", (int)priv->address);

      if (__verbs_init_context_cma(ctx, child_cm_id) != PHOTON_OK) {
        goto error_exit;
      }

      dbg_info("created context");

      if (args->pindex >= 0) {
        pindex = args->pindex;
      }
      else {
        pindex = (int)priv->address;
      }

      // save the child CM_ID in our process list
      ctx->cm_id[pindex] = child_cm_id;
      // save the QP in our process list
      ctx->qp[pindex] = child_cm_id->qp;

      memset(&conn_param, 0, sizeof conn_param);
      conn_param.responder_resources =ctx->atomic_depth;
      conn_param.initiator_depth = ctx->atomic_depth;
      // don't send any private data back
      conn_param.private_data = NULL;
      conn_param.private_data_len = 0;

      if (rdma_accept(child_cm_id, &conn_param)) {
        dbg_err("rdma_accept failed: %s", strerror(errno));
        goto error_exit;
      }
    }
    break;
    case RDMA_CM_EVENT_ESTABLISHED:
      dbg_info("connection established");
      num_connected++;
      break;
    default:
      dbg_err("bad event waiting for established %d", event->event);
      goto error_exit;
      break;
    }

    rdma_ack_cm_event(event);

  }
  while (num_connected < num_listeners);

  if (rdma_destroy_id(local_cm_id)) {
    dbg_err("Could not destroy CM ID");
  }

  if (args) {
    free(args);
  }

  dbg_info("Listener thread done");

error_exit:
  return NULL;
}

static int __verbs_connect_qps_cma(verbs_cnct_ctx *ctx, verbs_cnct_info *local_info, verbs_cnct_info *remote_info, int i, int num_qp) {
  struct addrinfo *res;
  struct addrinfo hints = {
    .ai_family   = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM
  };

  char *service, *host;
  int n, n_retries = RDMA_CMA_RETRIES;
  int port;
  struct rdma_cm_event *event;
  struct sockaddr_in sin;
  struct rdma_conn_param conn_param;

  // TODO: allow multiple RDMA CMA connections per process
  if (num_qp > 1) {
    dbg_err("photon does not currently support more than one CM_ID/QP per process!");
    goto error_exit;
  }

  // we may get a list of hostnames in the future
  // for now all we need is the remote ip and port
  host = inet_ntoa(remote_info[0].ip);
  port = remote_info[0].cma_port;

  dbg_info("connecting to %d (%s) on port %d", i, host, port);

  if (asprintf(&service, "%d", port) < 0)
    goto error_exit;

  if ((n = getaddrinfo(host, service, &hints, &res) < 0)) {
    dbg_err("%s for %s:%d", gai_strerror(n), host, port);
    goto error_exit;
  }

  sin.sin_addr.s_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);

  if (rdma_create_id(ctx->cm_schannel, &(ctx->cm_id[i]), NULL, RDMA_PS_TCP)) {
    dbg_err("Could not create CM ID");
    goto error_exit;
  }

retry_addr:
  if (rdma_resolve_addr(ctx->cm_id[i], NULL, (struct sockaddr *)&sin, RDMA_CMA_TIMEOUT)) {
    dbg_err("resolve addr failed: %s", strerror(errno));
    goto error_exit;
  }

  if (rdma_get_cm_event(ctx->cm_schannel, &event)) {
    goto error_exit;
  }

  if (event->event == RDMA_CM_EVENT_ADDR_ERROR && (n_retries-- > 0)) {
    rdma_ack_cm_event(event);
    goto retry_addr;
  }

  if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
    dbg_err("unexpected CM event (res0) %d", event->event);
    goto error_exit;
  }
  rdma_ack_cm_event(event);

retry_route:
  n_retries = RDMA_CMA_RETRIES;

  if (rdma_resolve_route(ctx->cm_id[i], RDMA_CMA_TIMEOUT)) {
    dbg_err("rdma_resolve_route failed: %s", strerror(errno));
    goto error_exit;
  }

  if (rdma_get_cm_event(ctx->cm_schannel, &event)) {
    goto error_exit;
  }

  if (event->event == RDMA_CM_EVENT_ROUTE_ERROR && (n_retries-- > 0)) {
    rdma_ack_cm_event(event);
    goto retry_route;
  }

  if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
    dbg_err("unexpected CM event (res1) %d", event->event);
    goto error_exit;
  }
  rdma_ack_cm_event(event);

  if (__verbs_init_context_cma(ctx, ctx->cm_id[i]) != PHOTON_OK) {
    goto error_exit;
  }

  // save the QP in our process list
  ctx->qp[i] = ctx->cm_id[i]->qp;

  struct rdma_cma_priv priv_data = {
    .address = _photon_myrank
  };

  memset(&conn_param, 0, sizeof conn_param);
  conn_param.responder_resources = ctx->atomic_depth;
  conn_param.initiator_depth = ctx->atomic_depth;
  conn_param.retry_count = RDMA_CMA_RETRIES;
  conn_param.private_data = &priv_data;
  conn_param.private_data_len = sizeof(struct rdma_cma_priv);

  if (rdma_connect(ctx->cm_id[i], &conn_param)) {
    dbg_err("rdma_connect failure: %s", strerror(errno));
    goto error_exit;
  }

  if (rdma_get_cm_event(ctx->cm_schannel, &event)) {
    goto error_exit;
  }

  if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
    dbg_err("unexpected CM event (est) %d", event->event);
    goto error_exit;
  }

  if (event->param.conn.private_data &&
      (event->param.conn.private_data_len > 0)) {
    // we got some data back from the remote peer
    // not used
  }
  rdma_ack_cm_event(event);

  freeaddrinfo(res);

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int __verbs_connect_qps(verbs_cnct_ctx *ctx, verbs_cnct_info *local_info, verbs_cnct_info *remote_info, int pindex, int num_qp) {
  int i;
  int err;
  char gid[40];

  for (i = 0; i < num_qp; ++i) {
    dbg_info("[%d/%d], pindex=%d lid=%x qpn=%x, psn=%x, qp[i].qpn=%x, gid=%s",
             _photon_myrank, _photon_nproc, pindex,
             remote_info[i].lid, remote_info[i].qpn, remote_info[i].psn,
             ctx->qp[pindex]->qp_num,
             inet_ntop(AF_INET6, remote_info[i].gid.raw, gid, 40));

    struct ibv_ah_attr ah_attr = {
      .is_global     = 0,
      .dlid          = remote_info[i].lid,
      .sl            = 0,
      .src_path_bits = 0,
      .port_num      = ctx->ib_port
    };
    
    if (remote_info[i].gid.global.interface_id) {
      ah_attr.is_global = 1;
      ah_attr.grh.hop_limit = 1;
      ah_attr.grh.dgid = remote_info[i].gid;
      ah_attr.grh.sgid_index = 0;
    }
    
    struct ibv_qp_attr attr = {
      .qp_state	          = IBV_QPS_RTR,
      .path_mtu	          = IBV_MTU_4096,
      .dest_qp_num        = remote_info[i].qpn,
      .rq_psn             = remote_info[i].psn,
      .max_dest_rd_atomic = 1,
      .min_rnr_timer	  = 12,
      .ah_attr = ah_attr
    };
    err=ibv_modify_qp(ctx->qp[pindex], &attr,
                      IBV_QP_STATE              |
                      IBV_QP_AV                 |
                      IBV_QP_PATH_MTU           |
                      IBV_QP_DEST_QPN           |
                      IBV_QP_RQ_PSN             |
                      IBV_QP_MAX_DEST_RD_ATOMIC |
                      IBV_QP_MIN_RNR_TIMER);
    if (err) {
      dbg_err("Failed to modify QP[%d] to RTR. Reason: %s", i, strerror(err));
      return PHOTON_ERROR;
    }

    attr.qp_state      = IBV_QPS_RTS;
    attr.timeout       = 14;
    attr.retry_cnt     = 7;
    attr.rnr_retry     = 7;
    attr.sq_psn        = local_info[i].psn;
    attr.max_rd_atomic = 1;
    err=ibv_modify_qp(ctx->qp[pindex], &attr,
                      IBV_QP_STATE     |
                      IBV_QP_TIMEOUT   |
                      IBV_QP_RETRY_CNT |
                      IBV_QP_RNR_RETRY |
                      IBV_QP_SQ_PSN    |
                      IBV_QP_MAX_QP_RD_ATOMIC);
    if (err) {
      dbg_err("Failed to modify QP[%d] to RTS. Reason: %s", i, strerror(err));
      return PHOTON_ERROR;
    }
  }

  return 0;
}

static verbs_cnct_info **__exch_cnct_info(verbs_cnct_ctx *ctx, verbs_cnct_info **local_info, int num_qp) {
  MPI_Request *rreq;
  MPI_Comm _photon_comm = __photon_config->comm;
  int peer;
  char smsg[ sizeof "00000000:00000000:00000000:00000000:00000000:00000000000000000000000000000000"];
  char **rmsg;
  int i, iproc;
  verbs_cnct_info **remote_info = ctx->remote_ci;
  int msg_size = sizeof smsg;
  char gid[sizeof "00000000000000000000000000000000" + 1];

  rreq = (MPI_Request *)malloc( _photon_nproc * sizeof(MPI_Request) );
  if( !rreq ) goto err_exit;

  rmsg = (char **)malloc( _photon_nproc * sizeof(char *) );
  if( !rmsg ) goto err_exit;
  for (iproc = 0; iproc < _photon_nproc; ++iproc) {
    rmsg[iproc] = (char *)malloc( msg_size );
    if( !rmsg[iproc] ) {
      int j = 0;
      for (j = 0; j < iproc; j++) {
        free(rmsg[j]);
      }
      free(rmsg);
      goto err_exit;
    }
  }

  for (i = 0; i < num_qp; ++i) {
    for (iproc=0; iproc < _photon_nproc; iproc++) {
      peer = (_photon_myrank+iproc)%_photon_nproc;
      if( peer == _photon_myrank ) {
        continue;
      }
      MPI_Irecv(rmsg[peer], msg_size, MPI_BYTE, peer, 0, _photon_comm, &rreq[peer]);

    }

    for (iproc=0; iproc < _photon_nproc; iproc++) {
      peer = (_photon_nproc+_photon_myrank-iproc)%_photon_nproc;
      if( peer == _photon_myrank ) {
        continue;
      }
      inet_ntop(AF_INET6, local_info[peer][i].gid.raw, gid, sizeof gid);
      sprintf(smsg, "%08x:%08x:%08x:%08x:%08d:%s", local_info[peer][i].lid, local_info[peer][i].qpn,
              local_info[peer][i].psn, local_info[peer][i].ip.s_addr, local_info[peer][i].cma_port, gid);
      //fprintf(stderr,"[%d/%d] Sending lid:qpn:psn:ip = %s to task=%d\n",_photon_myrank, _photon_nproc, smsg, peer);
      if( MPI_Send(smsg, msg_size , MPI_BYTE, peer, 0, _photon_comm ) != MPI_SUCCESS ) {
        dbg_err("Could not send local address");
        goto err_exit;
      }
    }

    for (iproc=0; iproc < _photon_nproc; iproc++) {
      peer = (_photon_myrank+iproc)%_photon_nproc;
      if( peer == _photon_myrank ) {
        continue;
      }

      if( MPI_Wait(&rreq[peer], MPI_STATUS_IGNORE) ) {
        dbg_err("Could not wait() to receive remote address");
        goto err_exit;
      }
      sscanf(rmsg[peer], "%x:%x:%x:%x:%d:%s",
             &remote_info[peer][i].lid,
             &remote_info[peer][i].qpn,
             &remote_info[peer][i].psn,
             &remote_info[peer][i].ip.s_addr,
             &remote_info[peer][i].cma_port,
             gid);
      inet_pton(AF_INET6, gid, &remote_info[peer][i].gid.raw);
    }
  }
  
  for (i = 0; i < _photon_nproc; i++) {
    free(rmsg[i]);
  }
  free(rmsg);

  return remote_info;
err_exit:
  return NULL;
}
