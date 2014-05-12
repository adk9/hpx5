#ifndef PHOTON_RDMA_INFO_LEDGER_H
#define PHOTON_RDMA_INFO_LEDGER_H

#include "photon_buffer.h"

typedef struct photon_ri_ledger_entry_t {
  volatile uint8_t header;
  uintptr_t addr;
  uint64_t size;
  struct photon_buffer_priv_t priv;
  uint32_t request;
  int tag;
  volatile uint16_t filler;
  volatile uint8_t footer;
} photon_ri_ledger_entry;

typedef struct photon_ri_ledger_t {
  photon_ri_ledger_entry *entries;
  int num_entries;
  int curr;
  struct photon_buffer_t remote;
} photon_ri_ledger;

typedef struct photon_ri_ledger_entry_t * photonRILedgerEntry;
typedef struct photon_ri_ledger_t * photonRILedger;

photonRILedger photon_ri_ledger_create_reuse(photonRILedgerEntry ledger_buffer, int ledger_size);
void photon_ri_ledger_free(photonRILedger ledger);

#endif
