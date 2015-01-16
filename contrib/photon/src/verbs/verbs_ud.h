#ifndef VERBS_UD_H
#define VERBS_UD_H

#include "verbs.h"
#include "verbs_connect.h"

PHOTON_INTERNAL int __verbs_ud_create_qp(verbs_cnct_ctx *ctx);
PHOTON_INTERNAL int __verbs_ud_attach_addr(verbs_cnct_ctx *ctx, union ibv_gid *gid);
PHOTON_INTERNAL int __verbs_ud_detach_addr(verbs_cnct_ctx *ctx, union ibv_gid *gid);
PHOTON_INTERNAL struct ibv_ah *__verbs_ud_create_ah(verbs_cnct_ctx *ctx, union ibv_gid *gid, int lid, struct ibv_ah **ret_ah);

#endif
