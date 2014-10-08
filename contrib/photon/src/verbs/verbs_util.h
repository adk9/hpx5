#ifndef VERBS_EXCHANGE_H
#define VERBS_EXCHANGE_H

#include "verbs.h"
#include "verbs_connect.h"

int __verbs_sync_qpn(verbs_cnct_ctx *ctx);
int __verbs_find_max_inline(verbs_cnct_ctx *ctx, int *ret_mi);

#endif
