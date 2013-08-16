#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "verbs.h"
#include "verbs_rdma_INFO_ledger.h"
#include "verbs_buffer.h"
#include "verbs_remote_buffer.h"

verbs_ri_ledger_t *verbs_ri_ledger_create_reuse(verbs_ri_ledger_entry_t *ledger_buffer, int ledger_size) {
	verbs_ri_ledger_t *new;

	new = malloc(sizeof(verbs_ri_ledger_t));
	if (!new) {
		log_err("verbs_ri_ledger_create(): couldn't allocate space for the ledger");
		goto error_exit;
	}

	bzero(new, sizeof(verbs_ri_ledger_t));

	new->entries = ledger_buffer;

	// we bzero this out so that valgrind doesn't believe these are
	// "uninitialized". They get filled in via RDMA so valgrind doesn't
	// know that the values have been filled in
	bzero(new->entries, sizeof(verbs_ri_ledger_entry_t) * ledger_size);

	new->curr = 0;
	new->num_entries = ledger_size;
	new->remote.request = NULL_COOKIE;

	return new;

error_exit:
	return NULL;
}

void verbs_ri_ledger_free(verbs_ri_ledger_t *ledger) {
	free(ledger);
}
