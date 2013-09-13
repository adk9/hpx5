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

#define MAX_CQ_ENTRIES      1000
#define RDMA_CMA_BASE_PORT  18000
#define RDMA_CMA_TIMEOUT    2000
#define RDMA_CMA_RETRIES    10

extern int _photon_myrank;
extern int _photon_nproc;
extern int _photon_forwarder;

static pthread_t cma_listener;

static verbs_cnct_info **__exch_cnct_info(verbs_cnct_ctx *ctx, verbs_cnct_info **local_info, int num_qp);
static int __verbs_connect_qps_cma(verbs_cnct_ctx *ctx, verbs_cnct_info **local_info, verbs_cnct_info **remote_info, int num_qp);
static int __verbs_connect_qps(verbs_cnct_ctx *ctx, verbs_cnct_info *local_info, verbs_cnct_info *remote_info, int pindex, int num_qp);
static int __verbs_init_context_cma(verbs_cnct_ctx *ctx, struct rdma_cm_id *cm_id);
static void *__rdma_cma_listener_thread(void *arg);

// data that gets exchanged during RDMA CMA connect
// for now we simply identify remote peers by address (rank)
struct rdma_cma_priv {
	uint64_t address;
};

int __verbs_init_context(verbs_cnct_ctx *ctx) {
	struct ibv_device **dev_list;
	struct ibv_context **ctx_list;
	int i, iproc, num_devs;

    if (__photon_config->use_cma) {
        ctx->cm_schannel = rdma_create_event_channel();
        if (!ctx->cm_schannel) {
			dbg_err("Could not create RDMA event channel");
            return PHOTON_ERROR;
        }

		ctx->cm_rchannel = rdma_create_event_channel();
        if (!ctx->cm_rchannel) {
            dbg_err("Could not create RDMA event channel");
            return PHOTON_ERROR;
        }
		
		// we will use this CM ID to listen for RDMA CMA connections
		if (rdma_create_id(ctx->cm_rchannel, &ctx->cm_id, NULL, RDMA_PS_TCP)) {
			dbg_err("Could not create CM ID");
			return PHOTON_ERROR;
		}

		// make sure there are some devices we can use
		ctx_list = rdma_get_devices(&num_devs);
		if (!num_devs) {
			dbg_err("No RDMA CMA devices found");
			return PHOTON_ERROR;
		}

		// initialize some QP info that gets set later
		for (iproc = 0; iproc < _photon_nproc; ++iproc) {
            ctx->verbs_processes[iproc].num_qp = MAX_QP;
			for (i = 0; i < MAX_QP; i++) {
				ctx->verbs_processes[iproc].qp[i] = NULL;
			}
		}
	
		// start the RDMA CMA listener
		pthread_create(&cma_listener, NULL, __rdma_cma_listener_thread, (void*)ctx);
	}
	else {
		dev_list = ibv_get_device_list(&num_devs);
		if (!num_devs) {
			dbg_err("No IB devices found");
			return PHOTON_ERROR;
		}
		
		for (i=0; i<=num_devs; i++) {
			if (!strcmp(ibv_get_device_name(dev_list[i]), ctx->ib_dev)) {
				ctr_info("using device %s:%d", ibv_get_device_name(dev_list[i]), ctx->ib_port);
				break;
			}
		}
		
		ctx->ib_context = ibv_open_device(dev_list[i]);
		if (!ctx->ib_context) {
			dbg_err("Could not get context for %s\n", ibv_get_device_name(dev_list[i]));
			return PHOTON_ERROR;
		}
		ctr_info("context has device %s", ibv_get_device_name(ctx->ib_context->device));

		// get my local lid
		struct ibv_port_attr attr;
		memset(&attr, 0, sizeof(attr));
		if(ibv_query_port(ctx->ib_context, ctx->ib_port, &attr) ) {
			dbg_err("Could not query port");
			return PHOTON_ERROR;
		}
		ctx->ib_lid = attr.lid;

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
					.max_wr	 = 500,
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
		for (iproc = 0; iproc < _photon_nproc; ++iproc) {
			
			//FIXME: What if I want to send to myself?
			if( iproc == _photon_myrank ) {
				continue;
			}
			
			ctx->verbs_processes[iproc].num_qp = MAX_QP;
			for (i = 0; i < MAX_QP; ++i) {
				struct ibv_qp_init_attr attr = {
					.qp_context = ctx,
					.send_cq    = ctx->ib_cq,
					.recv_cq    = ctx->ib_cq,
					.srq        = ctx->ib_srq,
					.cap	    = {
						.max_send_wr	= ctx->tx_depth,
						.max_send_sge = 1, // scatter gather element
						.max_recv_wr	= ctx->rx_depth,
						.max_recv_sge = 1, // scatter gather element
					},
					.qp_type    = IBV_QPT_RC
					//.sq_sig_all = 0
				};
				
				ctx->verbs_processes[iproc].qp[i] = ibv_create_qp(ctx->ib_pd, &attr);
				if (!(ctx->verbs_processes[iproc].qp[i])) {
					dbg_err("Could not create QP[%d] for task:%d", i, iproc);
					return PHOTON_ERROR;
				}
				
				{
					struct ibv_qp_attr attr;
					
					attr.qp_state    = IBV_QPS_INIT;
					attr.pkey_index	 = 0;
					attr.port_num	 = ctx->ib_port;
					attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;
					
					if (ibv_modify_qp(ctx->verbs_processes[iproc].qp[i], &attr,
									  IBV_QP_STATE		 |
									  IBV_QP_PKEY_INDEX	 |
									  IBV_QP_PORT		 |
									  IBV_QP_ACCESS_FLAGS)) {
						dbg_err("Failed to modify QP[%d] for task:%d to INIT", i, iproc);
						return PHOTON_ERROR;
					}
				}
			}
		}
	}
	
	return PHOTON_OK;
}

int __verbs_connect_peers(verbs_cnct_ctx *ctx) {
	verbs_cnct_info **local_info, **remote_info;
	struct ifaddrs *ifaddr, *ifa;
	int i, iproc;
	MPI_Comm _photon_comm = __photon_config->comm;

	ctr_info();

	local_info	= (verbs_cnct_info **)malloc( _photon_nproc*sizeof(verbs_cnct_info *) );
	if( !local_info ) {
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
	
	for(iproc=0; iproc<_photon_nproc; ++iproc) {

		if( iproc == _photon_myrank ) {
			continue;
		}

		local_info[iproc]	 = (verbs_cnct_info *)malloc( MAX_QP*sizeof(verbs_cnct_info) );
		if( !local_info[iproc] ) {
			goto error_exit;
		}

		for(i=0; i<MAX_QP; ++i) {
			if (__photon_config->use_cma) {
				local_info[iproc][i].qpn = 0x0;
			}
			else {
				local_info[iproc][i].qpn = ctx->verbs_processes[iproc].qp[i]->qp_num;
			}
			
			local_info[iproc][i].lid = ctx->ib_lid;
			local_info[iproc][i].psn = (lrand48() & 0xfff000) + _photon_myrank+1;
			local_info[iproc][i].ip = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
		}
	}

	remote_info = __exch_cnct_info(ctx, local_info, MAX_QP);
	if( !remote_info ) {
		dbg_err("Cannot exchange connect info");
		goto error_exit;
	}

	MPI_Barrier(_photon_comm);

	if (__photon_config->use_cma) {
		if (__verbs_connect_qps_cma(ctx, local_info, remote_info, MAX_QP)) {
			dbg_err("Could not connect queue pairs using RDMA CMA");
			goto error_exit;
		}
	}
	else {
		for (iproc = 0; iproc < _photon_nproc; ++iproc) {
			if( iproc == _photon_myrank ) {
				continue;
			}
			
			if(__verbs_connect_qps(ctx, local_info[iproc], remote_info[iproc], iproc, MAX_QP)) {
				dbg_err("verbs_connect_peers(): Cannot connect queue pairs");
				goto error_exit;
			}
		}
	}

	dbg_info("waiting for listener to finish...");
	pthread_join(cma_listener, NULL);
	dbg_info("DONE");

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

		ctx->ib_cq = ibv_create_cq(ctx->ib_context, ctx->tx_depth, ctx, ctx->ib_cc, 0);
		if (!ctx->ib_cq) {
			dbg_err("could not create CQ");
			goto error_exit;
		}
		
		if (ibv_req_notify_cq(ctx->ib_cq, 0)) {
			dbg_err("could not request CQ notifications");
			goto error_exit;
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
			.max_recv_wr  = ctx->tx_depth,
			.max_send_sge = 1,
			.max_recv_sge = 1,
			.max_inline_data = 0
		},
		.qp_type = IBV_QPT_RC,
		.sq_sig_all = 1,
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
	verbs_cnct_ctx *ctx = (verbs_cnct_ctx*)arg;
	int i, n;
    char *service;

    struct addrinfo *res;
    struct addrinfo hints = {
        .ai_flags    = AI_PASSIVE,
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };
    struct rdma_cm_event *event;
    struct sockaddr_in sin;
    struct rdma_cm_id *child_cm_id;
    struct rdma_conn_param conn_param;

	if (asprintf(&service, "%d", RDMA_CMA_BASE_PORT + _photon_myrank) < 0)
        goto error_exit;
	
    if ((n = getaddrinfo(NULL, service, &hints, &res)) < 0) {
        dbg_err("%s for port %d\n", gai_strerror(n), RDMA_CMA_BASE_PORT + _photon_myrank);
        goto error_exit;
    }

	sin.sin_addr.s_addr = 0;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(RDMA_CMA_BASE_PORT + _photon_myrank);

	if (rdma_bind_addr(ctx->cm_id, (struct sockaddr *)&sin)) {
		dbg_err("rdma_bind_addr failed: %s", strerror(errno));
		goto error_exit;
	}

	if (rdma_listen(ctx->cm_id, 0)) {
		dbg_err("rdma_listen failed: %s", strerror(errno));
		goto error_exit;
	}

	// accept some number of connections
	// this currently depends on rank position
	for (i=0; i<_photon_myrank; i++) {

		dbg_info("Listening for %d more connections on port %d", _photon_myrank - i, RDMA_CMA_BASE_PORT + _photon_myrank);

        if (rdma_get_cm_event(ctx->cm_rchannel, &event)) {
			dbg_err("did not get event");
			goto error_exit;
		}
		
        if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
            dbg_err("bad event waiting for connect request %d", event->event);
            goto error_exit;
		}
		
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
			//ctx->verbs_processes[priv->address].remote_priv = malloc(sizeof(struct rdma_cma_priv));
			//memcpy(ctx->verbs_processes[priv->address].remote_priv, priv, event->param.conn.private_data_len);
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

		// save the child CM_ID in our process list
		ctx->verbs_processes[i].cm_id = child_cm_id;
		// save the QP in our process list
		ctx->verbs_processes[i].qp[0] = child_cm_id->qp;
				
		memset(&conn_param, 0, sizeof conn_param);
        conn_param.responder_resources = ctx->tx_depth;
        conn_param.initiator_depth = ctx->tx_depth;
		// don't send any private data back
        conn_param.private_data = NULL;
        conn_param.private_data_len = 0;

        if (rdma_accept(child_cm_id, &conn_param)) {
            dbg_err("rdma_accept failed: %s", strerror(errno));
            goto error_exit;
        }
        rdma_ack_cm_event(event);

		if (rdma_get_cm_event(ctx->cm_rchannel, &event)) {
            dbg_err("did not get event");
            goto error_exit;
        }

        if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
            dbg_err("bad event waiting for connect request %d", event->event);
            goto error_exit;
        }
		rdma_ack_cm_event(event);
	}

	dbg_info("Listener thread done");

 error_exit:
	return NULL;
}

static int __verbs_connect_qps_cma(verbs_cnct_ctx *ctx, verbs_cnct_info **local_info, verbs_cnct_info **remote_info, int num_qp) {
    struct addrinfo *res;
    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    char *service, *host;
    int i, n;
    int n_retries = RDMA_CMA_RETRIES;
    struct rdma_cm_event *event;
    struct sockaddr_in sin;
    struct rdma_conn_param conn_param;

	// TODO: allow multiple RDMA CMA connections per process
	if (num_qp > 1) {
		dbg_err("photon does not currently support more than one CM_ID/QP per process!");
		goto error_exit;
	}

	sleep(1);

	for (i=(_photon_myrank+1); i<_photon_nproc; i++) {
		// we may get a list of hostnames in the future
		// for now we use the ip exchanged in the remote info
		host = inet_ntoa(remote_info[i][0].ip);

		dbg_info("connecting to %d (%s) on port %d", i, host, RDMA_CMA_BASE_PORT + i);

		if (asprintf(&service, "%d", RDMA_CMA_BASE_PORT + i) < 0)
			goto error_exit;
		
		if ((n = getaddrinfo(host, service, &hints, &res) < 0)) {
			dbg_err("%s for %s:%d", gai_strerror(n), host, RDMA_CMA_BASE_PORT + _photon_myrank);
			goto error_exit;
		}	   

		sin.sin_addr.s_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
		sin.sin_family = AF_INET;
        sin.sin_port = htons(RDMA_CMA_BASE_PORT + i);

		if (rdma_create_id(ctx->cm_schannel, &(ctx->verbs_processes[i].cm_id), NULL, RDMA_PS_TCP)) {
            dbg_err("Could not create CM ID");
			goto error_exit;
		}

	retry_addr:
		if (rdma_resolve_addr(ctx->verbs_processes[i].cm_id, NULL, (struct sockaddr *)&sin, RDMA_CMA_TIMEOUT)) {
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
            dbg_err("unexpected CM event %d", event->event);
            goto error_exit;
		}
        rdma_ack_cm_event(event);
		
	retry_route:
		n_retries = RDMA_CMA_RETRIES;
		
		if (rdma_resolve_route(ctx->verbs_processes[i].cm_id, RDMA_CMA_TIMEOUT)) {
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
            dbg_err("unexpected CM event %d", event->event);
            goto error_exit;
        }
        rdma_ack_cm_event(event);

		if (__verbs_init_context_cma(ctx, ctx->verbs_processes[i].cm_id) != PHOTON_OK) {
			goto error_exit;
		}

		// save the QP in our process list
		ctx->verbs_processes[i].qp[0] = ctx->verbs_processes[i].cm_id->qp;

		struct rdma_cma_priv priv_data = {
			.address = _photon_myrank
		};

		memset(&conn_param, 0, sizeof conn_param);
        conn_param.responder_resources = ctx->tx_depth;
		conn_param.initiator_depth = ctx->tx_depth;
        conn_param.retry_count = RDMA_CMA_RETRIES;
        conn_param.private_data = &priv_data;
		conn_param.private_data_len = sizeof(struct rdma_cma_priv);

        if (rdma_connect(ctx->verbs_processes[i].cm_id, &conn_param)) {
            dbg_err("rdma_connect failure: %s", strerror(errno));
            goto error_exit;
		}

        if (rdma_get_cm_event(ctx->cm_schannel, &event)) {
            goto error_exit;
		}

        if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
            dbg_err("unexpected CM event %d", event->event);
            goto error_exit;
		}

        if (event->param.conn.private_data &&
			(event->param.conn.private_data_len > 0)) {
			// we got some data back from the remote peer
			// not used
        }
		rdma_ack_cm_event(event);

		freeaddrinfo(res);
	}	

	return PHOTON_OK;

 error_exit:
	return PHOTON_ERROR;
}

static int __verbs_connect_qps(verbs_cnct_ctx *ctx, verbs_cnct_info *local_info, verbs_cnct_info *remote_info, int pindex, int num_qp) {
	int i;
	int err;
	ProcessInfo *verbs_process = &(ctx->verbs_processes[pindex]);

	for (i = 0; i < num_qp; ++i) {
		dbg_info("[%d/%d], i=%d lid=%x qpn=%x, psn=%x, qp[i].qpn=%x",
				 _photon_myrank, _photon_nproc, i,
				 remote_info[i].lid, remote_info[i].qpn, remote_info[i].psn,
				 verbs_process->qp[i]->qp_num);
		
		struct ibv_qp_attr attr = {
			.qp_state	        = IBV_QPS_RTR,
			.path_mtu	        = 3, // (3 == IBV_MTU_1024) which means 1024. Is this a good value?
			.dest_qp_num	    = remote_info[i].qpn,
			.rq_psn		        = remote_info[i].psn,
			.max_dest_rd_atomic = 1,
			.min_rnr_timer	    = 12,
			.ah_attr = {
				.is_global      = 0,
				.dlid	        = remote_info[i].lid,
				.sl	            = 0,
				.src_path_bits  = 0,
				.port_num       = ctx->ib_port
			}
		};
		err=ibv_modify_qp(verbs_process->qp[i], &attr,
		                  IBV_QP_STATE              |
		                  IBV_QP_AV                 |
		                  IBV_QP_PATH_MTU           |
		                  IBV_QP_DEST_QPN           |
		                  IBV_QP_RQ_PSN             |
		                  IBV_QP_MAX_DEST_RD_ATOMIC |
		                  IBV_QP_MIN_RNR_TIMER);
		if (err) {
			dbg_err("Failed to modify QP[%d] to RTR. Reason:%d", i, err);
			return PHOTON_ERROR;
		}

		attr.qp_state      = IBV_QPS_RTS;
		attr.timeout       = 14;
		attr.retry_cnt     = 7;
		attr.rnr_retry     = 7;
		attr.sq_psn        = local_info[i].psn;
		attr.max_rd_atomic = 1;
		err=ibv_modify_qp(verbs_process->qp[i], &attr,
		                  IBV_QP_STATE     |
		                  IBV_QP_TIMEOUT   |
		                  IBV_QP_RETRY_CNT |
		                  IBV_QP_RNR_RETRY |
		                  IBV_QP_SQ_PSN    |
		                  IBV_QP_MAX_QP_RD_ATOMIC);
		if (err) {
			dbg_err("Failed to modify QP[%d] to RTS. Reason:%d", i, err);
			return PHOTON_ERROR;
		}
	}

	return 0;
}

static verbs_cnct_info **__exch_cnct_info(verbs_cnct_ctx *ctx, verbs_cnct_info **local_info, int num_qp) {
	MPI_Request *rreq;
	MPI_Comm _photon_comm = __photon_config->comm;
	int peer;
	char smsg[ sizeof "00000000:00000000:00000000:00000000"];
	char **rmsg;
	int i, j, iproc;
	verbs_cnct_info **remote_info = NULL;
	int msg_size = sizeof "00000000:00000000:00000000:00000000";

	remote_info = (verbs_cnct_info **)malloc( _photon_nproc * sizeof(verbs_cnct_info *) );
	for (iproc = 0; iproc < _photon_nproc; ++iproc) {
		if( iproc == _photon_myrank ) {
			continue;
		}
		remote_info[iproc] = (verbs_cnct_info *)malloc( num_qp * sizeof(verbs_cnct_info) );
		if (!remote_info) {
			for (j = 0; j < iproc; j++) {
				free(remote_info[j]);
			}
			free(remote_info);
			goto err_exit;
		}
	}

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
			sprintf(smsg, "%08x:%08x:%08x:%08x", local_info[peer][i].lid, local_info[peer][i].qpn,
					local_info[peer][i].psn, local_info[peer][i].ip.s_addr);
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
			sscanf(rmsg[peer], "%x:%x:%x:%x",
				   &remote_info[peer][i].lid,
				   &remote_info[peer][i].qpn,
				   &remote_info[peer][i].psn,
				   &remote_info[peer][i].ip.s_addr);

			//fprintf(stderr,"[%d/%d] Received lid:qpn:psn:ip = %x:%x:%x:%s from task=%d\n",
			//		_photon_myrank, _photon_nproc,
			//		remote_info[peer][i].lid,
			//		remote_info[peer][i].qpn,
			//		remote_info[peer][i].psn,
			//		inet_ntoa(remote_info[peer][i].ip),
			//		peer);
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
