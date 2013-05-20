#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "dapl.h"
#include "dapl_rdma_info_ledger.h"
#include "dapl_buffer.h"
#include "dapl_remote_buffer.h"

dapl_ri_ledger_t *dapl_ri_ledger_create_reuse(dapl_ri_ledger_entry_t *ledger_buffer, int ledger_size) {
	dapl_ri_ledger_t *new;

	new = malloc(sizeof(dapl_ri_ledger_t));
	if (!new) {
		log_err("dapl_ri_ledger_create(): couldn't allocate space for the ledger");
		goto error_exit;
	}

	bzero(new, sizeof(dapl_ri_ledger_t));

	new->entries = ledger_buffer;

	// we bzero this out so that valgrind doesn't believe these are
	// "uninitialized". They get filled in via RDMA so valgrind doesn't
	// know that the values have been filled in
	bzero(new->entries, sizeof(dapl_ri_ledger_entry_t) * ledger_size);

	new->curr = 0;
	new->num_entries = ledger_size;
	new->remote.request = NULL_COOKIE;

	return new;

error_exit:
	return NULL;
}

void dapl_ri_ledger_free(dapl_ri_ledger_t *ledger) {
	free(ledger);
}
