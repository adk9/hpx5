#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "libphoton.h"
#include "photon_exchange.h"
#include "logging.h"

extern photonBuffer shared_storage;

int photon_setup_ri_ledgers(ProcessInfo *photon_processes, char *buf, int num_entries) {
  int i;
  int ledger_size, offset;

  dbg_info();

  ledger_size = sizeof(struct photon_ri_ledger_entry_t) * num_entries;

  // Allocate the receive info ledgers
  for(i = 0; i < (_photon_nproc + _photon_nforw); i++) {
    dbg_info("allocating rcv info ledger for %d: %p", i, (buf + ledger_size * i));
    dbg_info("Offset: %d", ledger_size * i);

    // allocate the ledger
    photon_processes[i].local_rcv_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + ledger_size * i), num_entries);
    if (!photon_processes[i].local_rcv_info_ledger) {
      log_err("couldn't create local rcv info ledger for process %d", i);
      return PHOTON_ERROR;
    }

    dbg_info("allocating remote ri ledger for %d: %p", i, buf + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i);
    dbg_info("Offset: %d", ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i);

    photon_processes[i].remote_rcv_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i), num_entries);
    if (!photon_processes[i].remote_rcv_info_ledger) {
      log_err("couldn't create remote rcv info ledger for process %d", i);
      return PHOTON_ERROR;
    }
  }

  // Allocate the send info ledgers
  offset = 2 * ledger_size * (_photon_nproc + _photon_nforw);
  for(i = 0; i < (_photon_nproc + _photon_nforw); i++) {
    dbg_info("allocating snd info ledger for %d: %p", i, (buf + offset + ledger_size * i));
    dbg_info("Offset: %d", offset + ledger_size * i);

    // allocate the ledger
    photon_processes[i].local_snd_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + offset + ledger_size * i), num_entries);
    if (!photon_processes[i].local_snd_info_ledger) {
      log_err("couldn't create local snd info ledger for process %d", i);
      return PHOTON_ERROR;
    }

    dbg_info("allocating remote ri ledger for %d: %p", i, buf + offset + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i);
    dbg_info("Offset: %d", offset + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i);

    photon_processes[i].remote_snd_info_ledger = photon_ri_ledger_create_reuse((photonRILedgerEntry) (buf + offset + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i), num_entries);
    if (!photon_processes[i].remote_snd_info_ledger) {
      log_err("couldn't create remote snd info ledger for process %d", i);
      return PHOTON_ERROR;
    }
  }

  return PHOTON_OK;
}

int photon_setup_FIN_ledger(ProcessInfo *photon_processes, char *buf, int num_entries) {
  int i;
  int ledger_size;

  dbg_info();

  ledger_size = sizeof(struct photon_rdma_FIN_ledger_entry_t) * num_entries;

  for(i = 0; i < (_photon_nproc + _photon_nforw); i++) {
    // allocate the ledger
    dbg_info("allocating local FIN ledger for %d", i);

    photon_processes[i].local_FIN_ledger = photon_rdma_FIN_ledger_create_reuse((photonFINLedgerEntry) (buf + ledger_size * i), num_entries);
    if (!photon_processes[i].local_FIN_ledger) {
      log_err("couldn't create local FIN ledger for process %d", i);
      return PHOTON_ERROR;
    }

    dbg_info("allocating remote FIN ledger for %d", i);

    photon_processes[i].remote_FIN_ledger = photon_rdma_FIN_ledger_create_reuse((photonFINLedgerEntry) (buf + ledger_size * (_photon_nproc + _photon_nforw) + ledger_size * i), num_entries);
    if (!photon_processes[i].remote_FIN_ledger) {
      log_err("couldn't create remote FIN ledger for process %d", i);
      return PHOTON_ERROR;
    }
  }

  return PHOTON_OK;
}

