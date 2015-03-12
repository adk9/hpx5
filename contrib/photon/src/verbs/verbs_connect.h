#ifndef VERBS_CONNECT_H
#define VERBS_CONNECT_H

#include "verbs.h"

typedef struct verbs_cnct_info_t {
  unsigned lid;
  unsigned qpn;
  unsigned psn;
  int cma_port;
  struct in_addr ip;
  union ibv_gid gid;
} verbs_cnct_info;

typedef struct verbs_cnct_ctx_t {
  char                      *ib_dev;
  int                        ib_port;
  struct ibv_context        *ib_context;
  struct ibv_pd             *ib_pd;
  struct ibv_cq            **ib_cq;
  struct ibv_cp             *ib_ud_cq;
  struct ibv_srq            *ib_srq;
  struct ibv_comp_channel   *ib_cc;
  int                        ib_lid;
  int                        ib_mtu;
  enum ibv_mtu               ib_mtu_attr;

  struct rdma_event_channel *cm_schannel;
  struct rdma_cm_id        **cm_id;

  struct ibv_qp            **qp;
  struct ibv_qp             *ud_qp;
  int                        psn;
  int                        num_qp;
  int                        use_ud;

  int                        tx_depth;
  int                        rx_depth;
  int                        atomic_depth;
  int                        max_sge;
  int                        max_inline;
  int                        max_srq_wr;
  int                        num_cq;

  verbs_cnct_info          **local_ci;
  verbs_cnct_info          **remote_ci;
} verbs_cnct_ctx;

PHOTON_INTERNAL int __verbs_init_context(verbs_cnct_ctx *ctx);
PHOTON_INTERNAL int __verbs_connect_peers(verbs_cnct_ctx *ctx);
PHOTON_INTERNAL int __verbs_connect_single(verbs_cnct_ctx *ctx, verbs_cnct_info *local_info,
					   verbs_cnct_info *remote_info, int pindex, verbs_cnct_info **ret_ci,
					   int *ret_len, photon_connect_mode_t mode);

#endif
