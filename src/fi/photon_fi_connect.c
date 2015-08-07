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

#include <rdma/fi_rma.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>

#include "photon_fi.h"
#include "photon_fi_connect.h"
#include "photon_exchange.h"
#include "util.h"
#include "logging.h"

#ifdef ENABLE_DEBUG
static int __print_short_info(struct fi_info *info)
{
  for (struct fi_info *cur = info; cur; cur = cur->next) {
    printf("%s: %s\n", cur->fabric_attr->prov_name, cur->fabric_attr->name);
    printf("    version: %d.%d\n", FI_MAJOR(cur->fabric_attr->prov_version),
	   FI_MINOR(cur->fabric_attr->prov_version));
    printf("    type: %s\n", fi_tostr(&cur->ep_attr->type, FI_TYPE_EP_TYPE));
    printf("    protocol: %s\n", fi_tostr(&cur->ep_attr->protocol, FI_TYPE_PROTOCOL));
  }
  return PHOTON_OK;
}

static int __print_long_info(struct fi_info *info) {
  for (struct fi_info *cur = info; cur; cur = cur->next) {
    printf("---\n");
    printf("%s", fi_tostr(cur, FI_TYPE_INFO));
  }
  return PHOTON_OK;
}
#endif

static int __fi_free_context(fi_cnct_ctx *ctx) {

  return PHOTON_OK;
}

static int __fi_alloc_context(fi_cnct_ctx *ctx, struct fi_info *fi) {
  int i, rc;

  // now initialize context space
  ctx->eps = (struct fid_ep**)malloc(_photon_nproc * sizeof(struct fid_ep*));
  if (!ctx->eps) {
    dbg_err("Could not allocate EP memory");
    goto error_exit;
  }

  ctx->cqs = (struct fid_cq**)malloc(ctx->num_cq * sizeof(struct fid_cq*));
  if (!ctx->cqs) {
    dbg_err("Could not allocate CQ memory");
    goto err1;
  }
  
  ctx->addrs = (fi_addr_t**)malloc(_photon_nproc * sizeof(fi_addr_t*));
  if (!ctx->addrs) {
    dbg_err("Could not allocate fi_addr memory");
    goto err2;
  }
  
  // create EPs
  for (i = _photon_myrank; i <= _photon_myrank; i++) {
    rc = fi_endpoint(ctx->dom, fi, &ctx->eps[i], NULL);
    if (rc) {
      dbg_err("Could not create endpoint number %d: %s", i, fi_strerror(-rc));
      goto err3;
    }
  }
  
  // create CQs
  struct fi_cq_attr cq_attr;
  memset(&cq_attr, 0, sizeof(cq_attr));
  cq_attr.format = FI_CQ_FORMAT_DATA;
  cq_attr.wait_obj = FI_WAIT_NONE;
  cq_attr.size = 128; // FIXME
  
  for (i = 0; i < ctx->num_cq; i++) {
    rc = fi_cq_open(ctx->dom, &cq_attr, &ctx->cqs[i], NULL);
    if (rc) {
      dbg_err("Could not open CQ number %d", i);
      goto err3;
    }
  }
  
  // create AV
  struct fi_av_attr av_attr;
  memset(&av_attr, 0, sizeof(av_attr));
  av_attr.type = fi->domain_attr->av_type ?
    fi->domain_attr->av_type : FI_AV_MAP;
  av_attr.count = _photon_nproc; // this AV will hold our peer addrs
  av_attr.name = NULL;

  rc = fi_av_open(ctx->dom, &av_attr, &ctx->av, NULL);
  if (rc) {
    dbg_err("Could not open AV");
    goto err3;
  }
  
  return PHOTON_OK;

 err3:
  free(ctx->addrs);
 err2:
  free(ctx->cqs);
 err1:
  free(ctx->eps);
 error_exit:
  return PHOTON_ERROR;
}

int __fi_init_context(fi_cnct_ctx *ctx) {
  struct fi_info *f;
  int rc;
  
  rc = fi_getinfo(FT_FIVERSION, ctx->node, ctx->service, ctx->flags,
		  ctx->hints, &ctx->fi);
  if (rc) {
    dbg_err("Could not get fi_info: %s", fi_strerror(-rc));
    goto error_exit;
  }

#ifdef ENABLE_DEBUG
  __print_long_info(ctx->fi);
#endif

  rc = fi_fabric(ctx->fi->fabric_attr, &ctx->fab, NULL);
  if (rc) {
    dbg_err("Could not init fabric: %s", fi_strerror(-rc));
    goto error_exit;
  }
  
  for (f = ctx->fi; f; f = f->next) {
    rc = fi_domain(ctx->fab, ctx->fi, &ctx->dom, NULL);
    if (rc) {
      dbg_info("Could not init domain using provider %s: %s",
	       f->fabric_attr->prov_name,
	       fi_strerror(-rc));
    }
    else {
      dbg_info("Created FI domain on %s : %s : %s",
	       f->fabric_attr->prov_name,
	       f->fabric_attr->name,
	       fi_tostr(&f->ep_attr->type, FI_TYPE_EP_TYPE));
      break;
    }
  }

  if (!f) {
    dbg_err("Could not use any libfabric providers!");
    goto err1;
  }

  rc = __fi_alloc_context(ctx, f);
  if (rc != PHOTON_OK) {
    goto err2;
  }
  
  rc = __fi_connect_peers(ctx, f);
  if (rc != PHOTON_OK) {
    goto err3;
  }

  fi_freeinfo(ctx->fi);

  return PHOTON_OK;
  
 err3:
  __fi_free_context(ctx);
 err2:
  fi_close(&ctx->dom->fid);
 err1:
  fi_close(&ctx->fab->fid);
 error_exit:
  fi_freeinfo(ctx->fi);
  return PHOTON_ERROR;
}

int __fi_connect_peers(fi_cnct_ctx *ctx, struct fi_info *fi) {
  uint8_t *sbuf;
  struct fid_ep *ep;
  int i, rc, cqind, offset;

  // create our "self" ep first to get local address
  ep = ctx->eps[_photon_myrank];
  cqind = PHOTON_GET_CQ_IND(ctx->num_cq, _photon_myrank);
  
  // bind CQ
  rc = fi_ep_bind(ep, &ctx->cqs[cqind]->fid, FI_WRITE);
  if (rc) {
    dbg_err("Could not bind CQ to self EP");
    goto error_exit;
  }
  
  // bind AV
  rc = fi_ep_bind(ep, &ctx->av->fid, 0);
  if (rc) {
    dbg_err("Could not bind AV to self EP");
    goto error_exit;
  }

  // enable EP
  rc = fi_enable(ep);
  if (rc) {
    dbg_err("Could not enable self EP");
    goto error_exit;
  }

  // set to zero for first call to get length
  ctx->addr_len = 0;
  rc = fi_getname(&ep->fid, ctx->local_addr, &ctx->addr_len);
  if (rc != -FI_ETOOSMALL) {
    dbg_err("Could not get endpoint addr size");
    goto error_exit;
  }
  
  ctx->local_addr = malloc(ctx->addr_len);
  rc = fi_getname(&ep->fid, ctx->local_addr, &ctx->addr_len);
  if (rc) {
    dbg_err("Could not get local endpoint addr");
    goto error_exit;
  }
  
  // create buffer for gathering addresses
  sbuf = (uint8_t*)malloc(_photon_nproc * ctx->addr_len);
  if (!sbuf) {
    dbg_err("Could not allocate gather buffer");
    goto error_exit;
  }

  // gather
  rc = photon_exchange_allgather(ctx->local_addr, sbuf, ctx->addr_len);
  if (rc != PHOTON_OK) {
    dbg_err("Could not gather EP addrs");
    goto err1;
  }

  // add addresses to AV and set remote addr handles
  for (i = 0; i < _photon_nproc; i++) {
    offset = i * ctx->addr_len;
    rc = fi_av_insert(ctx->av, &sbuf[offset], 1, ctx->addrs[i], 0,
		      &ctx->fi_ctx_av);
    if (rc != 1) {
      dbg_err("Could not insert addr %d of %d into AV: %s", i+1, _photon_nproc,
	      fi_strerror(rc));
      goto err1;
    }
  }
    
  free(sbuf);
  return PHOTON_OK;

 err1:
  free(sbuf);
 error_exit:
  return PHOTON_ERROR;
}

