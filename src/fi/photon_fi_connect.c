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

int __fi_init_context(fi_cnct_ctx *ctx) {
  struct fi_info *f;
  int rc;
  
  rc = fi_getinfo(FT_FIVERSION, ctx->node, ctx->service, ctx->flags,
		  ctx->hints, &ctx->fi);
  if (rc) {
    log_err("Could not get fi_info: %s", fi_strerror(rc));
    goto error_exit;
  }

#ifdef ENABLE_DEBUG
  __print_long_info(ctx->fi);
#endif

  rc = fi_fabric(ctx->fi->fabric_attr, &ctx->fab, NULL);
  if (rc) {
    log_err("Could not init fabric: %s", fi_strerror(rc));
    goto error_exit;
  }
  
  for (f = ctx->fi; f; f = f->next) {
    rc = fi_domain(ctx->fab, ctx->fi, &ctx->dom, NULL);
    if (rc) {
      dbg_info("Could not init domain using provider %s: %s",
	       f->fabric_attr->prov_name,
	       fi_strerror(rc));
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
    log_err("Could not use any libfabric providers!");
    goto err1;
  }

  return PHOTON_OK;
  
 err2:
  fi_close(&ctx->dom->fid);
 err1:
  fi_close(&ctx->fab->fid);
 error_exit:
  return PHOTON_ERROR;
}

int __fi_connect_peers(fi_cnct_ctx *ctx) {
  return PHOTON_OK;
}

