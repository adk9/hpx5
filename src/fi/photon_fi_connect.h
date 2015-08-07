#ifndef FI_CONNECT_H
#define FI_CONNECT_H

#include <rdma/fabric.h>
#include <rdma/fi_endpoint.h>

#include "photon_attributes.h"

typedef struct fi_cnct_ctx_t {
  struct fi_info         *fi;
  struct fi_info         *hints;
  struct fid_fabric      *fab;
  struct fid_domain      *dom;

  uint64_t                flags;
  char                   *node;
  char                   *service;
  char                   *domain;
  char                   *provider;

  int                     rdma_get_align;
  int                     rdma_put_align;
} fi_cnct_ctx;

PHOTON_INTERNAL int __fi_init_context(fi_cnct_ctx *ctx);
PHOTON_INTERNAL int __fi_connect_peers(fi_cnct_ctx *ctx);

#endif
