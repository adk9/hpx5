#ifndef PHOTON_UGNI_EXCHANGE_H
#define PHOTON_UGNI_EXCHANGE_H

#include "photon_ugni.h"

int __ugni_exchange_ri_ledgers(UgniProcessInfo *ugni_processes);
int __ugni_setup_ri_ledgers(UgniProcessInfo *ugni_processes, void *buf, int num_entries);
int __ugni_exchange_FIN_ledger(UgniProcessInfo *ugni_processes);
int __ugni_setup_FIN_ledger(UgniProcessInfo *ugni_processes, void *buf, int num_entries);

#endif
