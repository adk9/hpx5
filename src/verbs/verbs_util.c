#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "libphoton.h"
#include "logging.h"
#include "verbs_util.h"
#include "verbs_buffer.h"

int __verbs_sync_qpn(verbs_cnct_ctx *ctx) {
  unsigned *qp_numbers = (unsigned *)calloc(_photon_nproc, sizeof(unsigned));
  unsigned local_qpn;
  unsigned max_qpn = 0;
  struct ibv_qp *tmp_qp;
  int status, i;

  struct ibv_qp_init_attr attr = {
    .send_cq        = ctx->ib_cq,
    .recv_cq        = ctx->ib_cq,
    .cap            = {
      .max_send_wr     = 1,
      .max_send_sge    = 1,
      .max_recv_wr     = 1,
      .max_recv_sge    = 1,
    },
    .qp_type        = IBV_QPT_UD
  };
  
  tmp_qp = ibv_create_qp(ctx->ib_pd, &attr);
  if (!tmp_qp) {
    dbg_err("Could not create temp QP");
    return PHOTON_ERROR;
  }
  
  local_qpn = tmp_qp->qp_num;

  status = ibv_destroy_qp(tmp_qp);
  if (status) {
    dbg_err("Could not destroy temp QP: %s", strerror(status));
  }

  status = MPI_Allgather(&local_qpn, 1, MPI_UNSIGNED, qp_numbers, 1, MPI_UNSIGNED, MPI_COMM_WORLD);
  if (status != MPI_SUCCESS) {
    dbg_err("Could not allgather QP numbers");
    return PHOTON_ERROR;
  }
  
  for (i = 0; i < _photon_nproc; i++) {
    if (qp_numbers[i] > max_qpn) {
      max_qpn = qp_numbers[i];
    }
  }
  
  dbg_info("my qpn: %u, max qpn: %u", local_qpn, max_qpn);
  //printf("%d: my_qpn: %u, max_qpn: %u\n", _photon_myrank, local_qpn, max_qpn);

  if (local_qpn != max_qpn) {
    for (i = local_qpn; i <= max_qpn; i++) {
      tmp_qp = ibv_create_qp(ctx->ib_pd, &attr);
      local_qpn = tmp_qp->qp_num;
      status = ibv_destroy_qp(tmp_qp);
    }
  }

  dbg_info("new qpn: %u", local_qpn);
  //printf("%d: new qpn: %d\n", _photon_myrank, local_qpn);

  //exit(1);
  return PHOTON_OK;
}
