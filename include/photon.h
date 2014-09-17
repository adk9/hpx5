// =============================================================================
//  Photon RDMA Library (libphoton)
//
//  Copyright (c) 2014, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef PHOTON_H
#define PHOTON_H

#include <stdint.h>

struct photon_config_t {
  uint64_t address;         // i.e. process rank
  int nproc;                // Total number of processes

  struct {                  // OPTIONS for InfiniBand Verbs backend
    char *eth_dev;          // Device name for CMA mode
    char *ib_dev;           // IB device as listed by ibv_devinfo
    int ib_port;            // An active IB port
    int use_cma;            // Use connection manager to establish RDMA context
    int use_ud;             // EXPERIMENTAL: unreliable datagram mode (uses multicast)
    char *ud_gid_prefix;    // EXPERIMENTAL: GID prefix to use for UD multicast
  } ibv;
  
  struct {                  // OPTIONS for Cray UGNI backend (set -1 for defaults)
    char *eth_dev;          // Device name for PE
    int bte_thresh;         // Messages >= bytes use BTE (default 8192, set 0 to disable BTE)
  } ugni;

  struct {                  // EXPERIMENTAL: forwarder interface to XSPd
    int use_forwarder;
    char **forwarder_eids;
  } forwarder;

  struct {                  // Various buffer and message sizes (set -1 for defaults)
    int eager_buf_size;     // Size of eager buffer per rank in bytes (default 128k, set 0 to disable)
    int small_msg_size;     // Messages <= bytes will use eager buffers (default 8192, set 0 to disable)
    int ledger_entries;     // The number of ledger entries (default 64)
  } cap;

  void *comm;               // Optional communicator for use with MPI, etc.
  int meta_exch;            // See PHOTON_EXCH types below (default MPI)
  char *backend;            // "verbs" or "ugni"
};

typedef union photon_addr_t {
  uint8_t       raw[16];
  unsigned long s_addr;
  struct {
    uint64_t    prefix;
    uint64_t    proc_id;
  } global;
  struct {
    uint32_t    blk0;
    uint32_t    blk1;
    uint32_t    blk2;
    uint32_t    blk3;
  } blkaddr;
} photon_addr;

typedef uint64_t photon_rid;

// status for photon requests
struct photon_status_t {
  union photon_addr_t src_addr;
  photon_rid request;
  uint64_t size;
  int tag;
  int count;
  int error;
};

// registered buffer keys 
// current abstraction is two 64b values, this covers existing photon backends
// we don't want this to be ptr/size because then our ledger size is variable
struct photon_buffer_priv_t {
  uint64_t key0;
  uint64_t key1;
};

// use to track both local and remote buffers
struct photon_buffer_t {
  uintptr_t addr;
  uint64_t size;
  uint64_t offset;
  struct photon_buffer_priv_t priv;
};

typedef union photon_addr_t         * photonAddr;
typedef struct photon_config_t      * photonConfig;
typedef struct photon_status_t      * photonStatus;
typedef struct photon_buffer_priv_t * photonBufferPriv;
typedef struct photon_buffer_t      * photonBuffer;

#define PHOTON_OK              0x0000
#define PHOTON_ERROR_NOINIT    0x0001
#define PHOTON_ERROR           0x0002

#define PHOTON_EXCH_TCP        0x0000
#define PHOTON_EXCH_MPI        0x0001
#define PHOTON_EXCH_PMI        0x0002
#define PHOTON_EXCH_XSP        0x0003

#define PHOTON_SEND_LEDGER     0x0000
#define PHOTON_RECV_LEDGER     0x0001

#define PHOTON_REQ_NIL         0x0000
#define PHOTON_REQ_USERID      0x0001
#define PHOTON_REQ_NO_CQE      0x0002
#define PHOTON_REQ_ONE_CQE     0x0004

#define PHOTON_AMO_FADD        0x0001
#define PHOTON_AMO_CSWAP       0x0002

#define PHOTON_PROBE_ANY       0xffff
#define PHOTON_PROBE_EVQ       0x0001
#define PHOTON_PROBE_LEDGER    0x0002

#define PHOTON_ANY_TAG         -1
#define PHOTON_ANY_SOURCE      -1

int photon_initialized();
int photon_init(photonConfig cfg);
int photon_finalize();

// Buffers
int photon_register_buffer(void *buf, uint64_t size);
int photon_unregister_buffer(void *buf, uint64_t size);
int photon_get_buffer_private(void *buf, uint64_t size, photonBufferPriv ret_priv);
int photon_get_buffer_remote(photon_rid request, photonBuffer ret_buf);

// RDMA rendezvous
int photon_post_recv_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, photon_rid *request);
int photon_post_send_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, photon_rid *request);
int photon_post_send_request_rdma(int proc, uint64_t size, int tag, photon_rid *request);
int photon_wait_recv_buffer_rdma(int proc, int tag, photon_rid *request);
int photon_wait_send_buffer_rdma(int proc, int tag, photon_rid *request);
int photon_wait_send_request_rdma(int tag);
int photon_post_os_put(photon_rid request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset);
int photon_post_os_get(photon_rid request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset);
int photon_send_FIN(photon_rid request, int proc);

// RDMA one-sided
int photon_post_os_put_direct(int proc, void *ptr, uint64_t size, photonBuffer rbuf, int flags, photon_rid *request);
int photon_post_os_get_direct(int proc, void *ptr, uint64_t size, photonBuffer rbuf, int flags, photon_rid *request);

// Vectored one-sided put/get
int photon_post_os_putv_direct(int proc, void *ptr[], uint64_t size[], photonBuffer rbuf[], int nbuf,
                               int flags, photon_rid *request);
int photon_post_os_getv_direct(int proc, void *ptr[], uint64_t size[], photonBuffer rbuf[], int nbuf,
                               int flags, photon_rid *request);

// RDMA with completion
// If @p ptr is NULL, then only completion value in @p remote is sent
// The remote buffer is specified in @p rptr and the rkey in @p priv
int photon_put_with_completion(int proc, void *ptr, uint64_t size, void *rptr, struct photon_buffer_priv_t priv,
                               photon_rid local, photon_rid remote, int flags);
int photon_get_with_completion(int proc, void *ptr, uint64_t size, void *rptr, struct photon_buffer_priv_t priv,
                               photon_rid local, int flags);
// Can probe ANY_SOURCE but given @p proc will only poll the CQ (if available) and completion
// ledger associated with that rank
int photon_probe_completion(int proc, int *flag, photon_rid *request, int flags);

// Atomics
int photon_post_atomic(int proc, void *ptr, uint64_t val, int type, int flags, photon_rid *request);

// Checks
int photon_test(photon_rid request, int *flag, int *type, photonStatus status);
int photon_wait(photon_rid request);
int photon_wait_ledger(photon_rid request);
int photon_wait_any(int *ret_proc, photon_rid *ret_req);
int photon_wait_any_ledger(int *ret_proc, photon_rid *ret_req);
int photon_probe_ledger(int proc, int *flag, int type, photonStatus status);

// ==========================
// Experimental UD interface
// ==========================
int photon_send(photonAddr addr, void *ptr, uint64_t size, int flags, photon_rid *request);
int photon_recv(photon_rid request, void *ptr, uint64_t size, int flags);

// Tell photon that we want to accept messages for certain addresses
// identified by address family af
int photon_register_addr(photonAddr addr, int af);
int photon_unregister_addr(photonAddr addr, int af);

// Fill in addr with the local device address, using af as the hint
// default will be AF_INET6 and port gid
int photon_get_dev_addr(int af, photonAddr addr);

int photon_probe(photonAddr addr, int *flag, photonStatus status);

#endif
