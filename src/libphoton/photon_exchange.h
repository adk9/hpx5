#ifndef PHOTON_EXCHANGE
#define PHOTON_EXCHANGE

/* TODO: add general exchange methods */

int photon_setup_ri_ledgers(ProcessInfo *photon_processes, char *buf, int num_entries);
int photon_setup_FIN_ledger(ProcessInfo *verbs_processes, char *buf, int num_entries);

#endif
