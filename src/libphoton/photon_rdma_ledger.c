#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "photon_rdma_ledger.h"
#include "photon_buffer.h"
#include "photon_exchange.h"

photonLedger photon_rdma_ledger_create_reuse(photonLedgerEntry ledger_buffer, int num_entries) {
  photonLedger new;
  
  new = (struct photon_rdma_ledger_t *)((uintptr_t)ledger_buffer + PHOTON_LEDG_SSIZE(num_entries) -
					sizeof(struct photon_rdma_ledger_t));
  
  memset(new, 0, sizeof(struct photon_rdma_ledger_t));

  new->entries = ledger_buffer;

  // we bzero this out so that valgrind doesn't believe these are
  // "uninitialized". They get filled in via RDMA so valgrind doesn't
  // know that the values have been filled in
  memset(new->entries, 0, sizeof(struct photon_rdma_ledger_entry_t) * num_entries);

  new->curr = 0;
  new->tail = 0;
  new->rcur = 0;
  new->num_entries = num_entries;

  return new;
}

void photon_rdma_ledger_free(photonLedger ledger) {
  //free(ledger);
}

int photon_rdma_ledger_get_next(photonLedger l) {
  uint64_t curr, tail;
  curr = sync_fadd(&l->curr, 1, SYNC_RELAXED);
  tail = sync_load(&l->tail, SYNC_RELAXED);
  if ((curr - tail) > l->num_entries) {
    log_err("Exceeded number of outstanding ledger entries - increase ledger size or wait for completion");
    return -1;
  }
  return curr & (l->num_entries - 1);
}
