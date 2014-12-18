#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>

#include "logging.h"
#include "photon_rdma_ledger.h"
#include "photon_buffer.h"
#include "photon_exchange.h"

static int _get_remote_progress(int proc, photonLedger buf);

photonLedger photon_rdma_ledger_create_reuse(photonLedgerEntry ledger_buffer, int num_entries, int prefix) {
  photonLedger new;
  
  new = (struct photon_rdma_ledger_t *)((uintptr_t)ledger_buffer + PHOTON_LEDG_SSIZE(num_entries) -
					sizeof(struct photon_rdma_ledger_t));
  
  memset(new, 0, sizeof(struct photon_rdma_ledger_t));

  new->entries = ledger_buffer;

  // we bzero this out so that valgrind doesn't believe these are
  // "uninitialized". They get filled in via RDMA so valgrind doesn't
  // know that the values have been filled in
  memset(new->entries, 0, sizeof(struct photon_rdma_ledger_entry_t) * num_entries);

  new->prog = 0;
  new->curr = 0;
  new->tail = 0;
  new->num_entries = num_entries;
  new->acct.rcur = 0;
  new->acct.rloc = 0;
  new->acct.event_prefix = prefix;

  return new;
}

void photon_rdma_ledger_free(photonLedger ledger) {
  //free(ledger);
}

int photon_rdma_ledger_get_next(int proc, photonLedger l) {
  uint64_t curr, tail;
 
  do {
    curr = sync_load(&l->curr, SYNC_RELAXED);
    tail = sync_load(&l->tail, SYNC_RELAXED);
    if ((curr - tail) >= l->num_entries) {
      log_err("Exceeded number of outstanding ledger entries - increase ledger size or wait for completion");
      return -1;
    }
    if (((curr - l->acct.rcur)) >= l->num_entries &&
	l->acct.event_prefix != REQUEST_COOK_FIN) {  //XXX: don't wait for FIN ledger
      // receiver not ready, request an updated rcur
      _get_remote_progress(proc, l);
      dbg_trace("No new ledger entry until receiver catches up...");
      return -2;
    }
  } while (!sync_cas(&l->curr, curr, curr+1, SYNC_RELAXED, SYNC_RELAXED));

  if ((curr - l->acct.rcur) >= (l->num_entries * 0.8)) {
    // do a pro-active fetch of the remote ledger progress
    _get_remote_progress(proc, l);
  }
  
  return curr & (l->num_entries - 1);
}

static int _get_remote_progress(int proc, photonLedger buf) {
  int rc;
  uint32_t rloc;
  uint64_t cookie;
  uintptr_t rmt_addr;

  rloc = 0;
  if (sync_cas(&buf->acct.rloc, rloc, 1, SYNC_ACQUIRE, SYNC_RELAXED)) {
      
    dbg_trace("Fetching remote ledger curr at rcur: %llu", buf->acct.rcur);
    
    rmt_addr = buf->remote.addr + PHOTON_LEDG_SSIZE(buf->num_entries) -
      sizeof(struct photon_rdma_ledger_t) + offsetof(struct photon_rdma_ledger_t, prog); 
    
    cookie = ( (uint64_t)buf->acct.event_prefix<<32) | proc;
    
    rc = __photon_backend->rdma_get(proc, (uintptr_t)&buf->acct.rcur, rmt_addr, sizeof(buf->acct.rcur),
				    &(shared_storage->buf), &buf->remote, cookie, 0);
    if (rc != PHOTON_OK) {
      dbg_err("RDMA GET for remote ledger progress counter failed");
      return PHOTON_ERROR;
    }
  }

  return PHOTON_OK;
}
