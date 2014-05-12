#ifndef PHOTON_BACKEND_H
#define PHOTON_BACKEND_H

#include "photon.h"
#include "photon_io.h"
#include "photon_buffer.h"
#include "photon_msgbuffer.h"
#include "photon_rdma_INFO_ledger.h"
#include "photon_rdma_FIN_ledger.h"
#include "squeue.h"

#ifdef HAVE_XSP
#include "photon_xsp_forwarder.h"
#endif

#define NULL_COOKIE          0x0
#define DEF_QUEUE_LENGTH     5*512
#define DEF_NUM_REQUESTS     5*512

/* this should not exceed MCA max_qp_wr (typically 16k) */
#define LEDGER_SIZE          512
#define SMSG_SIZE            (4 * 4096)
#define MAX_BUF_ENTRIES      64

#define LEDGER               1
#define EVQUEUE              2
#define SENDRECV             3

#define REQUEST_NEW          0
#define REQUEST_PENDING      1
#define REQUEST_FAILED       2
#define REQUEST_COMPLETED    3

#define REQUEST_FLAG_NIL     0x00
#define REQUEST_FLAG_FIN     0x01

#define REQUEST_COOK_SEND    0xbeef
#define REQUEST_COOK_RECV    0xcafebabe

typedef enum { PHOTON_CONN_ACTIVE, PHOTON_CONN_PASSIVE } photon_connect_mode_t;

typedef struct proc_info_t {
  photonRILedger  local_snd_info_ledger;
  photonRILedger  remote_snd_info_ledger;
  photonRILedger  local_rcv_info_ledger;
  photonRILedger  remote_rcv_info_ledger;
  photonFINLedger local_FIN_ledger;
  photonFINLedger remote_FIN_ledger;

  photonMsgBuf    smsgbuf;

#ifdef HAVE_XSP
  libxspSess *sess;
  PhotonIOInfo *io_info;
#endif
} ProcessInfo;

/* photon transfer requests */
typedef struct photon_req_t {
  LIST_ENTRY(photon_req_t) list;
  SLIST_ENTRY(photon_req_t) slist;
  uint64_t id;
  int state;
  int flags;
  int type;
  int proc;
  int tag;
  int bentries[MAX_BUF_ENTRIES];
  int num_entries;
  uint64_t mmask;
  uint64_t length;
  photon_addr addr;
  struct photon_buffer_internal_t remote_buffer;
} photon_req;

typedef struct photon_req_t * photonRequest;

typedef struct photon_event_status_t {
  uint64_t id;
  int proc;
  void *priv;
} photon_event_status;

typedef struct photon_event_status_t * photonEventStatus;

/* photon memory registration requests */
struct photon_mem_register_req {
  SLIST_ENTRY(photon_mem_register_req) list;
  void *buffer;
  uint64_t buffer_size;
};

/* header for UD message */
typedef struct photon_ud_hdr_t {
  uint32_t request;
  uint32_t src_addr;
  uint32_t length;
  uint16_t msn;
  uint16_t nmsg;
} photon_ud_hdr;

/* TODO: fix parameters and generalize API */
struct photon_backend_t {
  void *context;
  int (*initialized)(void);
  int (*init)(photonConfig cfg, ProcessInfo *info, photonBI ss);
  int (*finalize)(void);
  int (*connect)(void *local_ci, void *remote_ci, int pindex, void **ret_ci, int *ret_len, photon_connect_mode_t);
  int (*get_info)(ProcessInfo *pi, int proc, void **info, int *size, photon_info_t type);
  int (*set_info)(ProcessInfo *pi, int proc, void *info, int size, photon_info_t type);
  /* API */
  int (*register_buffer)(void *buffer, uint64_t size);
  int (*unregister_buffer)(void *buffer, uint64_t size);
  int (*get_dev_addr)(int af, photonAddr addr);
  int (*register_addr)(photonAddr addr, int af);
  int (*unregister_addr)(photonAddr addr, int af);
  int (*test)(uint32_t request, int *flag, int *type, photonStatus status);
  int (*wait)(uint32_t request);
  int (*wait_ledger)(uint32_t request);
  int (*send)(photonAddr addr, void *ptr, uint64_t size, int flags, uint64_t *request);
  int (*recv)(uint64_t request, void *ptr, uint64_t size, int flags);
  int (*post_recv_buffer_rdma)(int proc, void *ptr, uint64_t size, int tag, uint32_t *request);
  int (*post_send_buffer_rdma)(int proc, void *ptr, uint64_t size, int tag, uint32_t *request);
  int (*post_send_request_rdma)(int proc, uint64_t size, int tag, uint32_t *request);
  int (*wait_recv_buffer_rdma)(int proc, int tag, uint32_t *request);
  int (*wait_send_buffer_rdma)(int proc, int tag, uint32_t *request);
  int (*wait_send_request_rdma)(int tag);
  int (*post_os_put)(uint32_t request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset);
  int (*post_os_get)(uint32_t request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset);
  int (*post_os_put_direct)(int proc, void *ptr, uint64_t size, int tag, photonBuffer rbuf, uint32_t *request);
  int (*post_os_get_direct)(int proc, void *ptr, uint64_t size, int tag, photonBuffer rbuf, uint32_t *request);
  int (*send_FIN)(uint32_t request, int proc);
  int (*wait_any)(int *ret_proc, uint32_t *ret_req);
  int (*wait_any_ledger)(int *ret_proc, uint32_t *ret_req);
  int (*probe_ledger)(int proc, int *flag, int type, photonStatus status);
  int (*probe)(photonAddr addr, int *flag, photonStatus status);
  int (*io_init)(char *file, int amode, MPI_Datatype view, int niter);
  int (*io_finalize)();
  /* data movement -- needs to be split out */
  int (*rdma_put)(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                  photonBuffer lbuf, photonBuffer rbuf, uint64_t id);
  int (*rdma_get)(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
                  photonBuffer lbuf, photonBuffer rbuf, uint64_t id);
  int (*rdma_send)(photonAddr addr, uintptr_t laddr, uint64_t size,
                   photonBuffer lbuf, uint64_t id);
  int (*rdma_recv)(photonAddr addr, uintptr_t laddr, uint64_t size,
                   photonBuffer lbuf, uint64_t id);
  int (*get_event)(photonEventStatus stat);
};

typedef struct photon_backend_t * photonBackend;

extern struct photon_backend_t photon_default_backend;

#ifdef HAVE_XSP
int photon_xsp_lookup_proc(libxspSess *sess, ProcessInfo **ret_pi, int *index);
int photon_xsp_unused_proc(ProcessInfo **ret_pi, int *index);
#endif

/* util */
int _photon_get_buffer_private(void *buf, uint64_t size, photonBufferPriv ret_priv);
int _photon_get_buffer_remote(uint32_t request, photonBuffer ret_buf);
int _photon_handle_addr(photonAddr addr, photonAddr ret_addr);

#endif
