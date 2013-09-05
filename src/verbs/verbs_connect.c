#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "libphoton.h"
#include "logging.h"
#include "verbs_connect.h"

extern int _photon_myrank;
extern int _photon_nproc;
extern int _photon_forwarder;

static verbs_cnct_info **__exch_cnct_info(verbs_cnct_ctx *ctx, verbs_cnct_info **local_info, int num_qp);
static int __verbs_connect_qps(verbs_cnct_ctx *ctx, verbs_cnct_info *local_info, verbs_cnct_info *remote_info, int pindex, int num_qp);

int __verbs_init_context(verbs_cnct_ctx *ctx) {
	struct ibv_device **dev_list;
	int i, iproc, num_qp, num_devs;

	ctr_info(" > verbs_init_context()");

	dev_list = ibv_get_device_list(&num_devs);
	if (!dev_list || !dev_list[0]) {
		ctr_info("No IB devices found\n");
		return 1;
	}

	for (i=0; i<=num_devs; i++) {
		if (!strcmp(ibv_get_device_name(dev_list[i]), ctx->ib_dev)) {
			ctr_info(" > verbs_init_context(): using device %s:%d", ibv_get_device_name(dev_list[i]), ctx->ib_port);
			break;
		}
	}

	ctx->ib_context = ibv_open_device(dev_list[i]);
	if (!ctx->ib_context) {
		ctr_info("Couldn't get context for %s\n", ibv_get_device_name(dev_list[i]));
		return 1;
	}
	ctr_info(" > verbs_init_context(): context has device %s", ibv_get_device_name(ctx->ib_context->device));

	ctx->ib_pd = ibv_alloc_pd(ctx->ib_context);
	if (!ctx->ib_pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		return 1;
	}
	
	{
		// get my local lid
		struct ibv_port_attr attr;
		memset(&attr, 0, sizeof(attr));
		if(ibv_query_port(ctx->ib_context, ctx->ib_port, &attr) ) {
			fprintf(stderr, "Cannot query port");
			return 1;
		}
		ctx->ib_lid = attr.lid;
	}
	
	// The second argument (cq_size) can be something like 40K.	 It should be
	// within NIC MaxCQEntries limit
	ctx->ib_cq = ibv_create_cq(ctx->ib_context, 1000, NULL, NULL, 0);
	if (!ctx->ib_cq) {
		fprintf(stderr, "Couldn't create CQ\n");
		return 1;
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
			fprintf(stderr, "Couldn't create SRQ\n");
			return 1;
		}
	}
	
	num_qp = MAX_QP;
	for (iproc = 0; iproc < _photon_nproc+_photon_forwarder; ++iproc) {

		//FIXME: What if I want to send to myself?
		if( iproc == _photon_myrank ) {
			continue;
		}

		ctx->verbs_processes[iproc].num_qp = num_qp;
		for (i = 0; i < num_qp; ++i) {
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
			};
			
			ctx->verbs_processes[iproc].qp[i] = ibv_create_qp(ctx->ib_pd, &attr);
			if (!(ctx->verbs_processes[iproc].qp[i])) {
				fprintf(stderr, "Couldn't create QP[%d] for task:%d\n", i, iproc);
				return 1;
			}
		}

		for (i = 0; i < num_qp; ++i) {
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
				fprintf(stderr, "Failed to modify QP[%d] for task:%d to INIT\n", i, iproc);
				return 1;
			}
		}
	}

	return 0;
}

int __verbs_connect_peers(verbs_cnct_ctx *ctx) {
	verbs_cnct_info **local_info, **remote_info;
	struct ifaddrs *ifaddr, *ifa;
	int i, iproc, num_qp;
	MPI_Comm _photon_comm = __photon_config->comm;

	ctr_info(" > verbs_connect_peers()");

	local_info	= (verbs_cnct_info **)malloc( _photon_nproc*sizeof(verbs_cnct_info *) );
	if( !local_info ) {
		goto error_exit;
	}

	if (getifaddrs(&ifaddr) == -1) {
		log_err("verbs_connect_peers(): Cannot get interface addrs");
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
		log_err("verbs_connect_peers(): Did not find interface info for %s\n", __photon_config->eth_dev);
		goto error_exit;
	}
	
	num_qp = MAX_QP;
	for(iproc=0; iproc<_photon_nproc; ++iproc) {

		if( iproc == _photon_myrank ) {
			continue;
		}

		local_info[iproc]	 = (verbs_cnct_info *)malloc( num_qp*sizeof(verbs_cnct_info) );
		if( !local_info[iproc] ) {
			goto error_exit;
		}

		for(i=0; i<num_qp; ++i) {
			local_info[iproc][i].lid = ctx->ib_lid;
			local_info[iproc][i].qpn = ctx->verbs_processes[iproc].qp[i]->qp_num;
			local_info[iproc][i].psn = (lrand48() & 0xfff000) + _photon_myrank+1;
			local_info[iproc][i].ip = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
		}
	}

	remote_info = __exch_cnct_info(ctx, local_info, num_qp);
	if( !remote_info ) {
		log_err("verbs_connect_peers(): Cannot exchange connect info");
		goto error_exit;
	}
	MPI_Barrier(_photon_comm);

	for (iproc = 0; iproc < _photon_nproc; ++iproc) {
		if( iproc == _photon_myrank ) {
			continue;
		}

		if(__verbs_connect_qps(ctx, local_info[iproc], remote_info[iproc], iproc, num_qp)) {
			log_err("verbs_connect_peers(): Cannot connect queue pairs");
			goto error_exit;
		}
	}

	return 0;

error_exit:
	return -1;
}

static int __verbs_connect_qps(verbs_cnct_ctx *ctx, verbs_cnct_info *local_info, verbs_cnct_info *remote_info, int pindex, int num_qp) {
	int i;
	int err;
	ProcessInfo *verbs_process = &(ctx->verbs_processes[pindex]);

	for (i = 0; i < num_qp; ++i) {
		fprintf(stderr,"[%d/%d], i=%d lid=%x qpn=%x, psn=%x, qp[i].qpn=%x\n",
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
			fprintf(stderr, "Failed to modify QP[%d] to RTR. Reason:%d\n", i,err);
			return 1;
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
			fprintf(stderr, "Failed to modify QP[%d] to RTS. Reason:%d\n", i, err);
			return 1;
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
				fprintf(stderr, "Couldn't send local address\n");
				goto err_exit;
			}
		}

		for (iproc=0; iproc < _photon_nproc; iproc++) {
			peer = (_photon_myrank+iproc)%_photon_nproc;
			if( peer == _photon_myrank ) {
				continue;
			}

			if( MPI_Wait(&rreq[peer], MPI_STATUS_IGNORE) ) {
				fprintf(stderr, "Couldn't wait() to receive remote address\n");
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
