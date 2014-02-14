#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "photon_rdma_FIN_ledger.h"
#include "photon_backend.h"
#include "photon_buffer.h"

photonFINLedger photon_rdma_FIN_ledger_create_reuse(photonFINLedgerEntry ledger_buffer, int num_entries) {
  photonFINLedger new;

  new = malloc(sizeof(struct photon_rdma_FIN_ledger_t));
  if (!new) {
    log_err("Could not allocate space for the ledger");
    goto error_exit;
  }

  memset(new, 0, sizeof(struct photon_rdma_FIN_ledger_t));

  new->entries = ledger_buffer;

  // we bzero this out so that valgrind doesn't believe these are
  // "uninitialized". They get filled in via RDMA so valgrind doesn't
  // know that the values have been filled in
  memset(new->entries, 0, sizeof(struct photon_rdma_FIN_ledger_entry_t) * num_entries);

  new->curr = 0;
  new->num_entries = num_entries;

  return new;

error_exit:
  return NULL;
}

void photon_rdma_FIN_ledger_free(photonFINLedger ledger) {
  free(ledger);
}
