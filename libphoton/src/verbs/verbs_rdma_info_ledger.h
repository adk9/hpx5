#ifndef DAPL_RDMA_INFO_LEDGER_H
#define DAPL_RDMA_INFO_LEDGER_H

#include "verbs_buffer.h"
#include "verbs_remote_buffer.h"

typedef struct verbs_remote_buffer_info {
	volatile uint8_t header;
	uint32_t request;
    uint32_t rkey;
    uintptr_t addr;
    uint32_t size;
	int tag;
	volatile uint16_t filler;
	volatile uint8_t footer;
} verbs_ri_ledger_entry_t;

typedef struct verbs_rdma_info_ledger_t {
	verbs_ri_ledger_entry_t *entries;
	int num_entries;
	verbs_remote_buffer_t remote;
	int curr;
} verbs_ri_ledger_t;

//verbs_ri_ledger_t *verbs_ri_ledger_create(int num_entries, DAT_IA_HANDLE ia, DAT_PZ_HANDLE pz);
verbs_ri_ledger_t *verbs_ri_ledger_create_reuse(verbs_ri_ledger_entry_t *ledger_buffer, int ledger_size);
void verbs_ri_ledger_free(verbs_ri_ledger_t *ledger);

#endif
