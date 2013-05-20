#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "dapl.h"
#include "dapl_rdma_FIN_ledger.h"
#include "dapl_buffer.h"
#include "dapl_remote_buffer.h"

dapl_rdma_FIN_ledger_t *dapl_rdma_FIN_ledger_create(int num_entries, DAT_IA_HANDLE ia, DAT_PZ_HANDLE pz) {
	dapl_rdma_FIN_ledger_t *new;

	new = malloc(sizeof(dapl_rdma_FIN_ledger_t));
	if (!new) {
		log_err("dapl_rdma_FIN_ledger_create(): couldn't allocate space for the ledger");
		goto error_exit;
	}

	new->entries = malloc(sizeof(dapl_rdma_FIN_ledger_entry_t) * num_entries);
	if (!new->entries) {
		log_err("dapl_rdma_FIN_ledger_create(): couldn't allocate space for the ledger entries");
		goto error_exit_ri;
	}

	new->local = dapl_buffer_create((char *) new->entries, sizeof(dapl_rdma_FIN_ledger_entry_t) * num_entries);
	if (!new->local) {
		log_err("dapl_rdma_FIN_ledger_create(): couldn't allocate local buffer for the ledger entries");
		goto error_exit_entries;
	}

	if (dapl_buffer_register(new->local, ia, pz) != 0) {
		log_err("dapl_rdma_FIN_ledger_create(): couldn't register local buffer for the ledger entries");
		goto error_exit_lbuff;
	}

	// we bzero this out so that valgrind doesn't believe these are
	// "uninitialized". They get filled in via RDMA so valgrind doesn't
	// know that the values have been filled in
	bzero(new->entries, sizeof(dapl_rdma_FIN_ledger_entry_t) * num_entries);

	new->curr = 0;
	new->num_entries = num_entries;
	new->remote.request = NULL_COOKIE;

	return new;

error_exit_lbuff:
	dapl_buffer_free(new->local);
error_exit_entries:
	free(new->entries);
error_exit_ri:
	free(new);
error_exit:
	return NULL;
}

dapl_rdma_FIN_ledger_t *dapl_rdma_FIN_ledger_create_reuse(dapl_rdma_FIN_ledger_entry_t *ledger_buffer, int num_entries) {
	dapl_rdma_FIN_ledger_t *new;

	new = malloc(sizeof(dapl_rdma_FIN_ledger_t));
	if (!new) {
		log_err("dapl_rdma_FIN_ledger_create(): couldn't allocate space for the ledger");
		goto error_exit;
	}

	bzero(new, sizeof(dapl_ri_ledger_t));

	new->entries = ledger_buffer;

	// we bzero this out so that valgrind doesn't believe these are
	// "uninitialized". They get filled in via RDMA so valgrind doesn't
	// know that the values have been filled in
	bzero(new->entries, sizeof(dapl_rdma_FIN_ledger_entry_t) * num_entries);

	new->curr = 0;
	new->num_entries = num_entries;
	new->remote.request = NULL_COOKIE;

	return new;

error_exit:
	return NULL;
}

void dapl_rdma_FIN_ledger_free(dapl_rdma_FIN_ledger_t *ledger) {
	if (ledger->local) {
		dapl_buffer_free(ledger->local);
		free(ledger->entries);
	}
	free(ledger);
}
