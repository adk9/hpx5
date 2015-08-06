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

int __fi_init_context(fi_cnct_ctx *ctx) {
  char *node, *service;
  uint64_t flags = 0;
  int rc;
  
  node = NULL;
  service = NULL;
  
  rc = fi_getinfo(FT_FIVERSION, node, service, flags, ctx->hints, &ctx->fi);
  if (rc) {
    log_err("Could not get fi_info: %s\n", fi_strerror(rc));
    goto error_exit;
  }

  __print_short_info(ctx->fi);

  rc = fi_fabric(ctx->fi->fabric_attr, &ctx->fab, NULL);
  if (rc) {
    log_err("Could not init fabric: %s", fi_strerror(rc));
    goto error_exit;
  }
  
  rc = fi_domain(ctx->fab, ctx->fi, &ctx->dom, NULL);
  if (rc) {
    log_err("Could init domain: %s", fi_strerror(rc));
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
