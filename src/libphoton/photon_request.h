#ifndef PHOTON_REQUEST_H
#define PHOTON_REQUEST_H

#include "photon_backend.h"
#include "photon_rdma_ledger.h"
#include "photon_rdma_INFO_ledger.h"
#include "bit_array/bit_array.h"

#define DEF_MAX_BUF_ENTRIES  64    // The number msgbuf entries for UD mode

#define REQUEST_NEW          0x01
#define REQUEST_PENDING      0x02
#define REQUEST_FAILED       0x03
#define REQUEST_COMPLETED    0x04

#define REQUEST_FLAG_NIL     0x00
#define REQUEST_FLAG_FIN     0x01
#define REQUEST_FLAG_EAGER   0x02
#define REQUEST_FLAG_EDONE   0x04
#define REQUEST_FLAG_LDONE   0x08
#define REQUEST_FLAG_USERID  0x10

#define PROC_REQUEST_ID(p, id) (((uint64_t)p<<32) | id)

typedef struct photon_req_t {
  uint32_t index;
  photon_rid id;
  int state;
  int flags;
  int type;
  int proc;
  int tag;
  int curr;
  int events;
  //int bentries[DEF_MAX_BUF_ENTRIES];
  //BIT_ARRAY *mmask;
  uint64_t length;
  photon_addr addr;
  struct photon_buffer_internal_t remote_buffer;
} photon_req;

typedef struct photon_req_table_t {
  uint32_t count;
  uint32_t size;
  uint32_t cind;
  uint32_t tind;
  struct photon_req_t *reqs;
} photon_req_table;

typedef struct photon_req_t       * photonRequest;
typedef struct photon_req_table_t * photonRequestTable;

PHOTON_INTERNAL photonRequest photon_get_request(int proc);
PHOTON_INTERNAL photonRequest photon_lookup_request(photon_rid rid);
PHOTON_INTERNAL int photon_free_request(photonRequest req);
PHOTON_INTERNAL int photon_count_request();

PHOTON_INTERNAL photonRequest photon_setup_request_direct(photonBuffer rbuf, int proc, int events);
PHOTON_INTERNAL photonRequest photon_setup_request_ledger_info(photonRILedgerEntry ri_entry, int curr, int proc);
PHOTON_INTERNAL photonRequest photon_setup_request_ledger_eager(photonLedgerEntry l_entry, int curr, int proc);
PHOTON_INTERNAL photonRequest photon_setup_request_send(photonAddr addr, int *bufs, int nbufs);
PHOTON_INTERNAL photonRequest photon_setup_request_recv(photonAddr addr, int msn, int msize, int bindex, int nbufs);

#endif
