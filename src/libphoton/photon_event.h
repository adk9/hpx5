#ifndef PHOTON_EVENT_H
#define PHOTON_EVENT_H

#include "photon_backend.h"

#define PHOTON_EVENT_OK        0x00
#define PHOTON_EVENT_ERROR     0x01
#define PHOTON_EVENT_NONE      0x02
#define PHOTON_EVENT_REQCOMP   0x04
#define PHOTON_EVENT_REQFOUND  0x05

#define PHOTON_EFLAG_NIL       0x00
#define PHOTON_EFLAG_LOCAL     0x01
#define PHOTON_EFLAG_REMOTE    0x02

PHOTON_INTERNAL int __photon_get_event(int proc, photon_rid *id);
PHOTON_INTERNAL int __photon_get_nevents(int proc, int max, photon_rid **id, int *n);
PHOTON_INTERNAL int __photon_get_revent(int proc, photon_rid *id, uint64_t *imm);
PHOTON_INTERNAL int __photon_get_nrevents(int proc, int max, photon_rid **id, uint64_t **imms, int *n);

PHOTON_INTERNAL int __photon_nbpop_event(photonRequest req);
PHOTON_INTERNAL int __photon_nbpop_sr(photonRequest req);
PHOTON_INTERNAL int __photon_nbpop_ledger(photonRequest req);
PHOTON_INTERNAL int __photon_wait_ledger(photonRequest req);
PHOTON_INTERNAL int __photon_wait_event(photonRequest req);
PHOTON_INTERNAL int __photon_try_one_event(photonRequest *rreq);

PHOTON_INTERNAL int __photon_handle_cq_special(photon_rid rid);
PHOTON_INTERNAL int __photon_handle_cq_event(photonRequest req, photon_rid rid, photonRequest *rreq);
PHOTON_INTERNAL int __photon_handle_send_event(photonRequest req, photon_rid rid);
PHOTON_INTERNAL int __photon_handle_recv_event(photon_rid id);

#endif
