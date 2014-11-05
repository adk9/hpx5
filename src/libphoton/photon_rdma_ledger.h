#ifndef PHOTON_RDMA_LEDGER_H
#define PHOTON_RDMA_LEDGER_H

#include "photon_buffer.h"

typedef struct photon_rdma_ledger_entry_t {
  volatile uint64_t request;
} photon_rdma_ledger_entry;

typedef struct photon_rdma_ledger_t {
  photon_rdma_ledger_entry *entries;
  uint64_t curr;
  uint64_t tail;
  uint32_t num_entries;
  struct photon_buffer_t remote;
} photon_rdma_ledger;

typedef struct photon_rdma_ledger_entry_t * photonLedgerEntry;
typedef struct photon_rdma_ledger_t * photonLedger;

PHOTON_INTERNAL photonLedger photon_rdma_ledger_create_reuse(photonLedgerEntry ledger_buffer, int num_entries);
PHOTON_INTERNAL void photon_rdma_ledger_free(photonLedger ledger);
PHOTON_INTERNAL int photon_rdma_ledger_get_next(photonLedger ledger);

#endif
