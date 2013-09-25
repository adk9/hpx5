#include <stdlib.h>
#include <string.h>

#include "photon_rdma_INFO_ledger.h"
#include "logging.h"

photonRILedger photon_ri_ledger_create_reuse(photonRILedgerEntry ledger_buffer, int ledger_size) {
	photonRILedger new;

	new = malloc(sizeof(struct photon_ri_ledger_t));
	if (!new) {
		log_err("couldn't allocate space for the ledger");
		goto error_exit;
	}

	memset(new, 0, sizeof(struct photon_ri_ledger_t));

	new->entries = ledger_buffer;

	// we bzero this out so that valgrind doesn't believe these are
	// "uninitialized". They get filled in via RDMA so valgrind doesn't
	// know that the values have been filled in
	memset(new->entries, 0, sizeof(struct photon_ri_ledger_entry_t) * ledger_size);

	new->curr = 0;
	new->num_entries = ledger_size;
	new->remote.request = NULL_COOKIE;

	return new;

error_exit:
	return NULL;
}

void photon_ri_ledger_free(photonRILedger ledger) {
	free(ledger);
}
