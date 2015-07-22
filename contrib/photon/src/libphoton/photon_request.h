#ifndef PHOTON_REQUEST_H
#define PHOTON_REQUEST_H

#include "photon_backend.h"
#include "photon_rdma_ledger.h"
#include "photon_rdma_INFO_ledger.h"
#include "bit_array/bit_array.h"
#include "libsync/locks.h"
#include "libsync/queues.h"

#define DEF_NR_LEVELS        (14)    // up to 14 levels of request buffers
#define DEF_NUM_REQUESTS     (1024)  // 1K pre-allocated requests per rank, power of 2
#define DEF_MAX_REQUESTS     (1<<20) // Allows up to 1M requests per pank

#define NULL_REQUEST         0x0
#define DEF_MAX_BUF_ENTRIES  64      // The number msgbuf entries for UD mode

#define REQUEST_NEW          0x01
#define REQUEST_PENDING      0x02
#define REQUEST_FAILED       0x03
#define REQUEST_COMPLETED    0x04
#define REQUEST_FREE         0x05

#define REQUEST_COOK_NIL     0xff000000
#define REQUEST_COOK_SEND    0xff100000
#define REQUEST_COOK_RECV    0xff200000
#define REQUEST_COOK_EAGER   0xff300000
#define REQUEST_COOK_ELEDG   0xff400000
#define REQUEST_COOK_PLEDG   0xff500000
#define REQUEST_COOK_EBUF    0xff600000
#define REQUEST_COOK_PBUF    0xff700000
#define REQUEST_COOK_FIN     0xff800000
#define REQUEST_COOK_SINFO   0xff900000
#define REQUEST_COOK_RINFO   0xffa00000
#define REQUEST_COOK_GPWC    0xffb00000

#define REQUEST_OP_DEFAULT   0x0000
#define REQUEST_OP_SENDBUF   (1<<1)
#define REQUEST_OP_SENDREQ   (1<<2)
#define REQUEST_OP_SENDFIN   (1<<3)
#define REQUEST_OP_RECVBUF   (1<<4)
#define REQUEST_OP_PWC       (1<<5)

#define REQUEST_FLAG_NIL     0x0000
#define REQUEST_FLAG_WFIN    (1<<1)
#define REQUEST_FLAG_EAGER   (1<<2)
#define REQUEST_FLAG_EDONE   (1<<3)
#define REQUEST_FLAG_LDONE   (1<<4)
#define REQUEST_FLAG_NO_LCE  (1<<5)
#define REQUEST_FLAG_NO_RCE  (1<<6)
#define REQUEST_FLAG_ROP     (1<<7)
#define REQUEST_FLAG_1PWC    (1<<8)
#define REQUEST_FLAG_2PWC    (1<<9)
#define REQUEST_FLAG_CMD     (1<<10)

#define MARK_DONE(e,s)         (sync_fadd(&e->tail, s, SYNC_RELAXED))
#define EB_MSG_SIZE(s)         (sizeof(struct photon_eb_hdr_t) + s + sizeof(uint8_t))
#define PROC_REQUEST_ID(p,id)  (((uint64_t)p<<32) | id)
#define IS_VALID_PROC(p)       ((p >= 0) && (p < _photon_nproc))

typedef struct photon_req_t {
  photon_rid id;
  int        proc;
  int        tag;
  uint16_t   op;
  uint16_t   type;
  uint16_t   state;
  uint16_t   flags;
  uint64_t   size;
  struct {
    struct photon_buffer_t buf;
    photon_rid             id;
  } local_info;
  struct {
    struct photon_buffer_t buf;
    photon_rid             id;
  } remote_info;
  struct {
    volatile uint16_t      events;
    uint16_t               rflags;
    uint64_t               cookie;
  } rattr;
  //int bentries[DEF_MAX_BUF_ENTRIES];
  //BIT_ARRAY *mmask;
} photon_req;

typedef struct photon_req_table_t {
  uint64_t  count;
  uint64_t  next;
  uint32_t *free;
  uint32_t  size;
  uint16_t  level;
  struct photon_req_t **reqs;
  two_lock_queue_t     *pwc_q;
  two_lock_queue_t     *gwc_q;
  two_lock_queue_t     *comp_q;
  volatile uint32_t     pcount;
  volatile uint32_t     gcount;
  tatas_lock_t          tloc;
} photon_req_table;

typedef struct photon_req_t       * photonRequest;
typedef struct photon_req_table_t * photonRequestTable;

PHOTON_INTERNAL int photon_request_init(photonConfig cfg);
PHOTON_INTERNAL photonRequest photon_get_request(int proc);
PHOTON_INTERNAL photonRequest photon_lookup_request(photon_rid rid);
PHOTON_INTERNAL int photon_free_request(photonRequest req);
PHOTON_INTERNAL int photon_count_request(int proc);

PHOTON_INTERNAL photonRequest photon_setup_request_direct(photonBuffer lbuf, photonBuffer rbuf, uint64_t size, int proc, int events);
PHOTON_INTERNAL photonRequest photon_setup_request_ledger_info(photonRILedgerEntry ri_entry, int curr, int proc);
PHOTON_INTERNAL photonRequest photon_setup_request_ledger_eager(photonLedgerEntry l_entry, int curr, int proc);
PHOTON_INTERNAL photonRequest photon_setup_request_send(photonAddr addr, int *bufs, int nbufs);
PHOTON_INTERNAL photonRequest photon_setup_request_recv(photonAddr addr, int msn, int msize, int bindex, int nbufs);

#endif
