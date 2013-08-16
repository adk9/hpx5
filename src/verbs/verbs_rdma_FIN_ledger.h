#ifndef VERBS_RDMA_SEND_LEDGER_H
#define VERBS_RDMA_SEND_LEDGER_H

#include "verbs_buffer.h"
#include "verbs_remote_buffer.h"

typedef struct verbs_rdma_FIN_ledger_entry {
	volatile uint8_t header;
	uint32_t request;
	volatile uint16_t filler;
	volatile uint8_t footer;
} verbs_rdma_FIN_ledger_entry_t;

typedef struct verbs_rdma_FIN_ledger_t {
	verbs_rdma_FIN_ledger_entry_t *entries;
	int num_entries;
	verbs_buffer_t *local;
	verbs_remote_buffer_t remote;
	int curr;
} verbs_rdma_FIN_ledger_t;

//verbs_rdma_FIN_ledger_t *verbs_rdma_FIN_ledger_create(int num_entries, DAT_IA_HANDLE ia, DAT_PZ_HANDLE pz);
verbs_rdma_FIN_ledger_t *verbs_rdma_FIN_ledger_create_reuse(verbs_rdma_FIN_ledger_entry_t *ledger_buffer, int num_entries);
void verbs_rdma_FIN_ledger_free(verbs_rdma_FIN_ledger_t *ledger);

#endif
