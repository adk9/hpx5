#ifndef PHOTON_H
#define PHOTON_H

#include <stdint.h>
#include <mpi.h>

struct photon_config_t {
  uint64_t address;
  int nproc;

  int use_cma;
  int use_forwarder;
  char **forwarder_eids;

  MPI_Comm comm;

  char *backend;
  char *mode;
  int meta_exch;

  char *eth_dev;
  char *ib_dev;
  int ib_port;
};

union photon_addr_t {
  uint8_t       raw[16];
  unsigned long s_addr;
  struct {
    uint64_t    subnet_prefix;
    uint64_t    proc_id;
  } global;
  struct {
    uint32_t    blk0;
    uint32_t    blk1;
    uint32_t    blk2;
    uint32_t    blk3;
  } blkaddr;
};

typedef union photon_addr_t photon_addr;

/* status for photon requests */
struct photon_status_t {
  union photon_addr_t src_addr;
  uint64_t size;
  int request;
  int tag;
  int count;
  int error;
};

/* registered buffer keys 
   current abstraction is two 64b values, this covers existing photon backends
   we don't want this to be ptr/size because then our ledger size is variable */
struct photon_buffer_priv_t {
  uint64_t key0;
  uint64_t key1;
};

/* use to track both local and remote buffers */
struct photon_buffer_t {
  uintptr_t addr;
  uint64_t size;
  struct photon_buffer_priv_t priv;
};

typedef struct photon_config_t * photonConfig;
typedef struct photon_status_t * photonStatus;
typedef struct photon_buffer_priv_t * photonBufferPriv;
typedef struct photon_buffer_t * photonBuffer;

#define PHOTON_OK              0x0000
#define PHOTON_ERROR_NOINIT    0x0001
#define PHOTON_ERROR           0x0002

#define PHOTON_EXCH_TCP        0x0000
#define PHOTON_EXCH_MPI        0x0001
#define PHOTON_EXCH_XSP        0x0002

#define PHOTON_SEND_LEDGER     0x0000
#define PHOTON_RECV_LEDGER     0x0001

#define PHOTON_ANY_SOURCE      -1

int photon_initialized();
int photon_init(photonConfig cfg);
int photon_finalize();

int photon_send(photon_addr addr, void *ptr, uint64_t size, int flags, uint32_t *request);
int photon_recv(uint32_t request, void *ptr, uint64_t size, int flags);

/* tell photon that we want to accept messages for certain addresses
   identified by address family af */
int photon_register_addr(photon_addr addr, int af);
int photon_unregister_addr(photon_addr addr, int af);

int photon_register_buffer(void *buf, uint64_t size);
int photon_unregister_buffer(void *buf, uint64_t size);
int photon_get_buffer_private(void *buf, uint64_t size, photonBufferPriv ret_priv);
int photon_get_buffer_remote(uint32_t request, photonBuffer ret_buf);
int photon_post_recv_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, uint32_t *request);
int photon_post_send_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, uint32_t *request);
int photon_post_send_request_rdma(int proc, uint64_t size, int tag, uint32_t *request);
int photon_wait_recv_buffer_rdma(int proc, int tag, uint32_t *request);
int photon_wait_send_buffer_rdma(int proc, int tag, uint32_t *request);
int photon_wait_send_request_rdma(int tag);
int photon_post_os_put(uint32_t request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset);
int photon_post_os_get(uint32_t request, int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset);
int photon_post_os_put_direct(int proc, void *ptr, uint64_t size, int tag, photonBuffer rbuf, uint32_t *request);
int photon_post_os_get_direct(int proc, void *ptr, uint64_t size, int tag, photonBuffer rbuf, uint32_t *request);
int photon_send_FIN(uint32_t request, int proc);
int photon_test(uint32_t request, int *flag, int *type, photonStatus status);

int photon_wait(uint32_t request);
int photon_wait_ledger(uint32_t request);

int photon_wait_any(int *ret_proc, uint32_t *ret_req);
int photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req);

int photon_probe_ledger(int proc, int *flag, int type, photonStatus status);
int photon_probe(photon_addr addr, int *flag, int type, photonStatus status);

#endif
