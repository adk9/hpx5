#ifndef PHOTON_REQUEST_H
#define PHOTON_REQUEST_H

#include "photon_backend.h"

PHOTON_INTERNAL photonRequest __photon_get_request();
PHOTON_INTERNAL int __photon_setup_request_direct(photonBuffer rbuf, int proc, int tag, int entries, photon_rid rid, photon_rid eid);
PHOTON_INTERNAL int __photon_setup_request_ledger_info(photonRILedgerEntry ri_entry, int curr, int proc, photon_rid *request);
PHOTON_INTERNAL int __photon_setup_request_ledger_eager(photonLedgerEntry l_entry, int curr, int proc, photon_rid *request);
PHOTON_INTERNAL int __photon_setup_request_send(photonAddr addr, int *bufs, int nbufs, photon_rid request);
PHOTON_INTERNAL photonRequest __photon_setup_request_recv(photonAddr addr, int msn, int msize, int bindex, int nbufs, photon_rid request);

#endif
