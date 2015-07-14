#ifndef VERBS_EXCHANGE_H
#define VERBS_EXCHANGE_H

#include "verbs.h"
#include "verbs_connect.h"

PHOTON_INTERNAL int __verbs_post_srq_recv(verbs_cnct_ctx *ctx, uint64_t id, int proc, int num);
PHOTON_INTERNAL int __verbs_post_rq_recv(verbs_cnct_ctx *ctx, uint64_t id, int proc, int num);
PHOTON_INTERNAL int __verbs_sync_qpn(verbs_cnct_ctx *ctx);
PHOTON_INTERNAL int __verbs_find_max_inline(verbs_cnct_ctx *ctx, int *ret_mi);

#endif
