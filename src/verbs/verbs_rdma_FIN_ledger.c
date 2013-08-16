#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "verbs.h"
#include "verbs_rdma_FIN_ledger.h"
#include "verbs_buffer.h"
#include "verbs_remote_buffer.h"

verbs_rdma_FIN_ledger_t *verbs_rdma_FIN_ledger_create_reuse(verbs_rdma_FIN_ledger_entry_t *ledger_buffer, int num_entries) {
	verbs_rdma_FIN_ledger_t *new;

	new = malloc(sizeof(verbs_rdma_FIN_ledger_t));
	if (!new) {
		log_err("verbs_rdma_FIN_ledger_create_reuse(): couldn't allocate space for the ledger");
		goto error_exit;
	}

	bzero(new, sizeof(verbs_ri_ledger_t));

	new->entries = ledger_buffer;

	// we bzero this out so that valgrind doesn't believe these are
	// "uninitialized". They get filled in via RDMA so valgrind doesn't
	// know that the values have been filled in
	bzero(new->entries, sizeof(verbs_rdma_FIN_ledger_entry_t) * num_entries);

	new->curr = 0;
	new->num_entries = num_entries;
	new->remote.request = NULL_COOKIE;

	return new;

error_exit:
	return NULL;
}

void verbs_rdma_FIN_ledger_free(verbs_rdma_FIN_ledger_t *ledger) {
	if (ledger->local) {
		verbs_buffer_free(ledger->local);
		free(ledger->entries);
	}
	free(ledger);
}
