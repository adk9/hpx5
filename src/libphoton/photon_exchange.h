#ifndef PHOTON_EXCHANGE
#define PHOTON_EXCHANGE

int photon_exchange_ri_ledgers(ProcessInfo *processes);
int photon_exchange_FIN_ledger(ProcessInfo *processes);
int photon_exchange_eager_buf(ProcessInfo *processes);

int photon_setup_ri_ledgers(ProcessInfo *processes, char *buf, int num_entries);
int photon_setup_FIN_ledger(ProcessInfo *processes, char *buf, int num_entries);
int photon_setup_EAGER_buf(ProcessInfo *processes, char *buf, int num_entries);

#endif
