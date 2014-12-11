#ifndef PHOTON_RDMA_LEDGER_H
#define PHOTON_RDMA_LEDGER_H

#include "photon_buffer.h"

typedef struct photon_rdma_ledger_entry_t {
  volatile uint64_t request;
} photon_rdma_ledger_entry;

typedef struct photon_rdma_ledger_t {
  uint64_t curr;
  uint64_t tail;
  uint32_t num_entries;
  photon_rdma_ledger_entry *entries;
  struct photon_buffer_t remote;
  struct photon_acct_t   acct;
} photon_rdma_ledger;

typedef struct photon_rdma_ledger_entry_t * photonLedgerEntry;
typedef struct photon_rdma_ledger_t * photonLedger;

PHOTON_INTERNAL photonLedger photon_rdma_ledger_create_reuse(photonLedgerEntry ledger_buffer, int num_entries, int prefix);
PHOTON_INTERNAL void photon_rdma_ledger_free(photonLedger ledger);
PHOTON_INTERNAL int photon_rdma_ledger_get_next(int proc, photonLedger ledger);

#endif
