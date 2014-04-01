#ifndef VERBS_EXCHANGE_H
#define VERBS_EXCHANGE_H

#include "verbs.h"
#include "verbs_connect.h"

int __verbs_sync_qpn(verbs_cnct_ctx *ctx);
int __verbs_exchange_ri_ledgers(ProcessInfo *verbs_processes);
int __verbs_exchange_FIN_ledger(ProcessInfo *verbs_processes);

#endif
