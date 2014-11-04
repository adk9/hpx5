#ifndef PHOTON_EVENT_H
#define PHOTON_EVENT_H

#include "photon_backend.h"

PHOTON_INTERNAL int __photon_nbpop_event(photonRequest req);
PHOTON_INTERNAL int __photon_nbpop_sr(photonRequest req);
PHOTON_INTERNAL int __photon_nbpop_ledger(photonRequest req);
PHOTON_INTERNAL int __photon_wait_ledger(photonRequest req);
PHOTON_INTERNAL int __photon_wait_event(photonRequest req);

PHOTON_INTERNAL int __photon_handle_cq_event(photonRequest req, photon_rid rid);
PHOTON_INTERNAL int __photon_handle_send_event(photonRequest req, photon_rid rid);
PHOTON_INTERNAL int __photon_handle_recv_event(photon_rid id);

#endif
