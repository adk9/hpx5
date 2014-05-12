#ifndef PHOTON_RDMA_SEND_LEDGER_H
#define PHOTON_RDMA_SEND_LEDGER_H

#include "photon_buffer.h"

typedef struct photon_rdma_FIN_ledger_entry_t {
  volatile uint8_t header;
  uint32_t request;
  volatile uint16_t filler;
  volatile uint8_t footer;
} photon_rdma_FIN_ledger_entry;

typedef struct photon_rdma_FIN_ledger_t {
  photon_rdma_FIN_ledger_entry *entries;
  int num_entries;
  struct photon_buffer_t remote;
  int curr;
} photon_rdma_FIN_ledger;

typedef struct photon_rdma_FIN_ledger_entry_t * photonFINLedgerEntry;
typedef struct photon_rdma_FIN_ledger_t * photonFINLedger;

photonFINLedger photon_rdma_FIN_ledger_create_reuse(photonFINLedgerEntry ledger_buffer, int num_entries);
void photon_rdma_FIN_ledger_free(photonFINLedger ledger);

#endif
