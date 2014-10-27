#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <mpi.h>
#include "gni_pub.h"
#include "utility_functions.h"
#include "logging.h"

#define BIND_ID_MULTIPLIER       100
#define CDM_ID_MULTIPLIER        1000
#define POST_ID_MULTIPLIER       10
#define MAX_CQ_ENTRIES           1000
#define GEMINI_DEVICE_ID         0x0

struct ledger {
  uint64_t a;
  uint64_t b;
};

typedef struct {
  gni_mem_handle_t mdh;
  uint64_t         addr;
} mdh_addr_t;

/* Global */
int _photon_nproc, _photon_myrank;
FILE *_phot_ofp;
int _photon_start_debugging = 1;

int main(int argc, char **argv) {
  /* GNI_CDM_MODE_BTE_SINGLE_CHANNEL | GNI_CDM_MODE_DUAL_EVENTS | GNI_CDM_MODE_FMA_SHARED */

  gni_cdm_handle_t    cdm_handle;
  gni_nic_handle_t    nic_handle;
  gni_cq_handle_t     local_cq_handle;
  gni_cq_handle_t     remote_cq_handle;
  gni_ep_handle_t    *ep_handles;
  mdh_addr_t          send_mem_handle;
  mdh_addr_t          recv_mem_handle;
  mdh_addr_t         *mem_handles;

  int rank, size;
  int cookie, status, i;
  int send_to, receive_from;
  int modes = 0;
  uint8_t ptag;
  uint32_t cdm_id;
  uint32_t bind_id;
  unsigned local_addr;
  unsigned *nic_addrs;
  unsigned local_dev;
  struct ledger *send_buf;
  struct ledger *recv_buf;

  MPI_Request *req;

  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  /* so we can use existing logging */
  _photon_nproc = size;
  _photon_myrank = rank;

  nic_addrs = (unsigned *)calloc(size, sizeof(unsigned));
  send_buf = (struct ledger *)calloc(size, sizeof(struct ledger));
  recv_buf = (struct ledger *)calloc(size, sizeof(struct ledger));

  send_mem_handle.addr = (uint64_t)send_buf;
  recv_mem_handle.addr = (uint64_t)recv_buf;

  ptag = get_ptag();
  cookie = get_cookie();

  cdm_id = rank * CDM_ID_MULTIPLIER + CDM_ID_MULTIPLIER;

  status = GNI_CdmCreate(cdm_id, ptag, cookie, modes, &cdm_handle);
  if (status != GNI_RC_SUCCESS) {
    dbg_err("GNI_CdmCreate ERROR status: %s (%d)", gni_err_str[status], status);
    goto error_exit;
  }

  dbg_trace("GNI_CdmCreate inst_id: %i ptag: %u cookie: 0x%x", cdm_id, ptag, cookie);

  status = GNI_CdmAttach(cdm_handle, GEMINI_DEVICE_ID, &local_dev, &nic_handle);
  if (status != GNI_RC_SUCCESS) {
    dbg_err("GNI_CdmAttach ERROR status: %s (%d)", gni_err_str[status], status);
    goto error_exit;
  }

  dbg_trace("Attached to GEMINI PE: %d", local_dev);

  /* setup completion queue for local events */
  status = GNI_CqCreate(nic_handle, MAX_CQ_ENTRIES, 0, GNI_CQ_NOBLOCK, NULL, NULL, &local_cq_handle);
  if (status != GNI_RC_SUCCESS) {
    dbg_err("GNI_CqCreate local_cq ERROR status: %s (%d)\n", gni_err_str[status], status);
    goto error_exit;
  }

  /* setup completion queue for remote memory events */
  status = GNI_CqCreate(nic_handle, MAX_CQ_ENTRIES, 0, GNI_CQ_NOBLOCK, NULL, NULL, &remote_cq_handle);
  if (status != GNI_RC_SUCCESS) {
    dbg_err("GNI_CqCreate remote_cq ERROR status: %s (%d)\n", gni_err_str[status], status);
    goto error_exit;
  }

  mem_handles = (mdh_addr_t *)calloc(size, sizeof(mdh_addr_t));
  if (!mem_handles) {
    dbg_err("Could not allocate memory handle array");
    goto error_exit;
  }

  ep_handles = (gni_ep_handle_t *)calloc(size, sizeof(gni_ep_handle_t));
  if (!ep_handles) {
    dbg_err("Could not allocate endpoint handle array");
    goto error_exit;
  }

  local_addr = get_gni_nic_address(GEMINI_DEVICE_ID);
  status = MPI_Allgather(&local_addr, 1, MPI_UNSIGNED, nic_addrs, 1, MPI_UNSIGNED, MPI_COMM_WORLD);
  if (status != MPI_SUCCESS) {
    dbg_err("Could not allgather NIC IDs");
    goto error_exit;
  }

  for (i=0; i<size; i++) {

    if (rank == i)
      continue;

    status = GNI_EpCreate(nic_handle, local_cq_handle, &ep_handles[i]);
    if (status != GNI_RC_SUCCESS) {
      dbg_err("GNI_EpCreate ERROR status: %s (%d)", gni_err_str[status], status);
      goto error_exit;
    }
    dbg_trace("GNI_EpCreate remote rank: %4i NIC: %p, CQ: %p, EP: %p", i, nic_handle,
             local_cq_handle, ep_handles[i]);


    bind_id = (rank * BIND_ID_MULTIPLIER) + BIND_ID_MULTIPLIER + i;

    status = GNI_EpBind(ep_handles[i], nic_addrs[i], bind_id);
    if (status != GNI_RC_SUCCESS) {
      dbg_err("GNI_EpBind ERROR status: %s (%d)", gni_err_str[status], status);
      goto error_exit;
    }
    dbg_trace("GNI_EpBind   remote rank: %4i EP:  %p remote_address: %u, remote_id: %u", i,
             ep_handles[i], nic_addrs[i], bind_id);
  }

  status = GNI_MemRegister(nic_handle, (uint64_t)send_buf, size*sizeof(struct ledger),
                           NULL, GNI_MEM_READWRITE, -1, &send_mem_handle.mdh);
  if (status != GNI_RC_SUCCESS) {
    dbg_err("GNI_MemRegister ERROR status: %s (%d)", gni_err_str[status], status);
    goto error_exit;
  }
  dbg_trace("GNI_MemRegister SEND size: %lu address: %p", size*sizeof(struct ledger), send_buf);

  status = GNI_MemRegister(nic_handle, (uint64_t)recv_buf, size*sizeof(struct ledger),
                           remote_cq_handle, GNI_MEM_READWRITE, -1, &recv_mem_handle.mdh);
  if (status != GNI_RC_SUCCESS) {
    dbg_err("GNI_MemRegister ERROR status: %s (%d)", gni_err_str[status], status);
    goto error_exit;
  }
  dbg_trace("GNI_MemRegister RECV size: %lu address: %p", size*sizeof(struct ledger), recv_buf);


  dbg_trace("My receive buffer (0x%lx) mem handle: 0x%016lx / 0x%016lx", (uint64_t)recv_buf,
           recv_mem_handle.mdh.qword1, recv_mem_handle.mdh.qword2);

  req = (MPI_Request *)malloc(size * sizeof(MPI_Request));
  for(i=0; i <size; i++) {
    if( MPI_Irecv(&mem_handles[i], sizeof(mdh_addr_t), MPI_BYTE, i, 0, MPI_COMM_WORLD, &req[i]) != MPI_SUCCESS ) {
      dbg_err("Couldn't post irecv() for remote mem handle from task %d", i);
      goto error_exit;
    }
  }

  for(i=0; i<size; i++) {
    if( MPI_Send(&recv_mem_handle, sizeof(mdh_addr_t), MPI_BYTE, i, 0, MPI_COMM_WORLD) != MPI_SUCCESS) {
      dbg_err("Couldn't send receive mem handle to process %d", i);
      goto error_exit;
    }
  }

  if (MPI_Waitall(size, req, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
    dbg_err("Couldn't wait() for all receive mem handles");
    goto error_exit;
  }

  for (i=0; i<size; i++) {
    dbg_trace("Remote recv buf (0x%lx) mem handle for rank %d: 0x%016lx / 0x%016lx", mem_handles[i].addr, i,
             mem_handles[i].mdh.qword1, mem_handles[i].mdh.qword2);
  }

  send_to = (rank + 1) % size;
  receive_from = (size + rank - 1) % size;

  /* we'll just send the first entry */
  send_buf[0].a = 0xdeadbeef;
  send_buf[0].b = rank;

  for (i=0; i<size; i++) {
    if (i==rank)
      continue;

    dbg_trace("Sending to %d, receiving from %d", send_to, receive_from);

    gni_post_descriptor_t fma_desc;
    gni_post_descriptor_t *event_post_desc_ptr;
    gni_cq_entry_t current_event;
    unsigned event_inst_id;
    unsigned send_post_id = rank * POST_ID_MULTIPLIER;

    fma_desc.type = GNI_POST_FMA_PUT;
    fma_desc.cq_mode = GNI_CQMODE_GLOBAL_EVENT | GNI_CQMODE_REMOTE_EVENT;
    fma_desc.dlvr_mode = GNI_DLVMODE_PERFORMANCE;
    fma_desc.local_addr = (uint64_t) send_buf;
    fma_desc.local_mem_hndl = send_mem_handle.mdh;
    fma_desc.remote_addr = mem_handles[send_to].addr + (send_to * sizeof(struct ledger));
    fma_desc.remote_mem_hndl = mem_handles[send_to].mdh;
    fma_desc.length = sizeof(struct ledger);
    fma_desc.post_id = send_post_id;

    status = GNI_PostFma(ep_handles[send_to], &fma_desc);
    if (status != GNI_RC_SUCCESS) {
      dbg_err("GNI_PostFma data ERROR status: %s (%d)\n", gni_err_str[status], status);
      continue;
    }

    dbg_trace("GNI_PostFma data transfer: %4i successful", i);
    dbg_trace("data transfer complete, checking CQ events");

    status = get_cq_event(local_cq_handle, 1, 1, &current_event);
    if (status == 0) {
      status = GNI_GetCompleted(local_cq_handle, current_event, &event_post_desc_ptr);
      if (status != GNI_RC_SUCCESS) {
        dbg_err("GNI_GetCompleted  data ERROR status: %s (%d)", gni_err_str[status], status);
      }
      if (send_post_id != event_post_desc_ptr->post_id) {
        dbg_err("Completed data ERROR received post_id: %lu, expected post_id: %u",
                event_post_desc_ptr->post_id, send_post_id);
      }
      else {
        dbg_trace("GNI_GetCompleted  data transfer: %4i send to: %4i remote addr: 0x%lx post_id: %lu", i, send_to,
                 event_post_desc_ptr->remote_addr,
                 event_post_desc_ptr->post_id);
      }

      event_inst_id = GNI_CQ_GET_INST_ID(current_event);
      dbg_trace("Got event ID: %u", event_inst_id);

    }
    else {
      /* rc == 2 is an overrun */
      dbg_err("Error getting CQ event: %d\n", status);
    }
  }

  /* wait until we see our recv buffer get set */
  volatile uint64_t *check_ptr = (uint64_t *) & recv_buf[rank].a;
  while (*check_ptr != 0xdeadbeef) {
    printf("Rank %d waiting for remote node to PUT...\n", rank);
    sleep(1);
  }

  printf("Rank %d got 0x%lx from rank %lu\n", rank, *check_ptr, recv_buf[rank].b);

  MPI_Barrier(MPI_COMM_WORLD);

  /* could do some cleanup here... */

  return 0;

error_exit:
  return -1;
}
