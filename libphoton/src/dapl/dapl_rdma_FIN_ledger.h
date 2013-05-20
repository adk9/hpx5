#ifndef DAPL_RDMA_SEND_LEDGER_H
#define DAPL_RDMA_SEND_LEDGER_H

#include "dapl_buffer.h"
#include "dapl_remote_buffer.h"

typedef struct dapl_rdma_FIN_ledger_entry {
	volatile uint8_t header;
	uint32_t request;
	volatile uint16_t filler;
	volatile uint8_t footer;
} dapl_rdma_FIN_ledger_entry_t;

typedef struct dapl_rdma_FIN_ledger_t {
	dapl_rdma_FIN_ledger_entry_t *entries;
	int num_entries;
	dapl_buffer_t *local;
	dapl_remote_buffer_t remote;
	int curr;
} dapl_rdma_FIN_ledger_t;

dapl_rdma_FIN_ledger_t *dapl_rdma_FIN_ledger_create(int num_entries, DAT_IA_HANDLE ia, DAT_PZ_HANDLE pz);
dapl_rdma_FIN_ledger_t *dapl_rdma_FIN_ledger_create_reuse(dapl_rdma_FIN_ledger_entry_t *ledger_buffer, int num_entries);
void dapl_rdma_FIN_ledger_free(dapl_rdma_FIN_ledger_t *ledger);

#endif
