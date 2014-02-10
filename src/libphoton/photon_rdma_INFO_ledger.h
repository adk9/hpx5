#ifndef PHOTON_RDMA_INFO_LEDGER_H
#define PHOTON_RDMA_INFO_LEDGER_H

#include "photon_buffer.h"

typedef struct photon_ri_ledger_entry_t {
  volatile uint8_t header;
  uint32_t request;
  uint32_t rkey;
  uintptr_t addr;
  uint32_t size;
  int tag;
  uint64_t qword1;
  uint64_t qword2;
  volatile uint16_t filler;
  volatile uint8_t footer;
} photon_ri_ledger_entry;

typedef struct photon_ri_ledger_t {
  photon_ri_ledger_entry *entries;
  int num_entries;
  photon_remote_buffer remote;
  int curr;
} photon_ri_ledger;

typedef struct photon_ri_ledger_entry_t * photonRILedgerEntry;
typedef struct photon_ri_ledger_t * photonRILedger;

photonRILedger photon_ri_ledger_create_reuse(photonRILedgerEntry ledger_buffer, int ledger_size);
void photon_ri_ledger_free(photonRILedger ledger);

#endif
