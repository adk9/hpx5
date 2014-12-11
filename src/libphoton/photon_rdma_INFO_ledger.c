#include <stdlib.h>
#include <string.h>

#include "photon_rdma_INFO_ledger.h"
#include "photon_exchange.h"
#include "logging.h"

photonRILedger photon_ri_ledger_create_reuse(photonRILedgerEntry ledger_buffer, int ledger_size, int prefix) {
  photonRILedger new;

  new = (struct photon_ri_ledger_t *)((uintptr_t)ledger_buffer + PHOTON_INFO_SSIZE(ledger_size) -
				      sizeof(struct photon_ri_ledger_t));

  memset(new, 0, sizeof(struct photon_ri_ledger_t));

  new->entries = ledger_buffer;

  // we bzero this out so that valgrind doesn't believe these are
  // "uninitialized". They get filled in via RDMA so valgrind doesn't
  // know that the values have been filled in
  memset(new->entries, 0, sizeof(struct photon_ri_ledger_entry_t) * ledger_size);

  new->curr = 0;
  new->num_entries = ledger_size;
  new->acct.rcur = 0;
  new->acct.rloc = 0;
  new->acct.event_prefix = prefix;

  return new;
}

void photon_ri_ledger_free(photonRILedger ledger) {
  //free(ledger);
}

int photon_ri_ledger_get_next(photonRILedger l) {
  uint64_t curr, tail;
  curr = sync_fadd(&l->curr, 1, SYNC_RELAXED);
  tail = sync_load(&l->tail, SYNC_RELAXED);
  if ((curr - tail) > l->num_entries) {
    log_err("Exceeded number of outstanding RI ledger entries - increase ledger size or wait for completion");
    return -1;
  }  
  return curr % l->num_entries;
}
