#ifndef DAPL_RDMA_INFO_LEDGER_H
#define DAPL_RDMA_INFO_LEDGER_H

#include "dapl_buffer.h"
#include "dapl_remote_buffer.h"

typedef struct dapl_remote_buffer_info {
	volatile uint8_t header;
	uint32_t request;
	DAT_RMR_CONTEXT context;
	DAT_VADDR address;
	uint32_t size;
	int tag;
	volatile uint16_t filler;
	volatile uint8_t footer;
} dapl_ri_ledger_entry_t;

typedef struct dapl_rdma_info_ledger_t {
	dapl_ri_ledger_entry_t *entries;
	int num_entries;
	dapl_remote_buffer_t remote;
	int curr;
} dapl_ri_ledger_t;

dapl_ri_ledger_t *dapl_ri_ledger_create(int num_entries, DAT_IA_HANDLE ia, DAT_PZ_HANDLE pz);
dapl_ri_ledger_t *dapl_ri_ledger_create_reuse(dapl_ri_ledger_entry_t *ledger_buffer, int ledger_size);
void dapl_ri_ledger_free(dapl_ri_ledger_t *ledger);

#endif
