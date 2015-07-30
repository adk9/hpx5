#ifndef PHOTON_PWC_H
#define PHOTON_PWC_H

#include "libphoton.h"
#include "photon_request.h"

#define PWC_ALIGN      8
#define PWC_BUFFER     0x01
#define PWC_LEDGER     0x02

#define PWC_COMMAND_MASK     ((uint64_t)0xff<<56)
#define PWC_COMMAND_PWC_REQ  ((uint64_t)0x01<<56)
#define PWC_COMMAND_PWC_REP  ((uint64_t)0x02<<56)

typedef struct pwc_recv_ledger_t {
  uint64_t   flag;
  photon_rid rid;
} pwc_recv_ledger;

// interface to deal with pwc used along with post/put/get/test
PHOTON_INTERNAL int photon_pwc_init(photonConfig cfg);
PHOTON_INTERNAL int photon_pwc_add_req(photonRequest req);
PHOTON_INTERNAL photonRequest photon_pwc_pop_req(int proc);

PHOTON_INTERNAL int _photon_put_with_completion(int proc, uint64_t size,
						photonBuffer lbuf,
						photonBuffer rbuf,
						photon_rid local, photon_rid remote,
						int flags);
PHOTON_INTERNAL int _photon_get_with_completion(int proc, uint64_t size,
						photonBuffer lbuf,
						photonBuffer rbuf,
						photon_rid local, photon_rid remote,
						int flags);
PHOTON_INTERNAL int _photon_probe_completion(int proc, int *flag, int *remaining,
					     photon_rid *request, int *src, int flags);

#endif
