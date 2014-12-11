#ifndef PHOTON_RDMA_INFO_LEDGER_H
#define PHOTON_RDMA_INFO_LEDGER_H

#include "photon_buffer.h"

typedef struct photon_ri_ledger_entry_t {
  volatile uint8_t header;
  photon_rid request;
  uintptr_t addr;
  uint64_t size;
  struct photon_buffer_priv_t priv;
  uint32_t tag;
  uint32_t flags;
  volatile uint8_t footer;
} photon_ri_ledger_entry;

typedef struct photon_ri_ledger_t {
  photon_ri_ledger_entry *entries;
  uint64_t curr;
  uint64_t tail;
  uint32_t num_entries;
  struct photon_buffer_t remote;
  struct photon_acct_t   acct;
} photon_ri_ledger;

typedef struct photon_ri_ledger_entry_t * photonRILedgerEntry;
typedef struct photon_ri_ledger_t * photonRILedger;

PHOTON_INTERNAL photonRILedger photon_ri_ledger_create_reuse(photonRILedgerEntry ledger_buffer, int ledger_size, int prefix);
PHOTON_INTERNAL void photon_ri_ledger_free(photonRILedger ledger);
PHOTON_INTERNAL int photon_ri_ledger_get_next(photonRILedger ledger);

#endif
