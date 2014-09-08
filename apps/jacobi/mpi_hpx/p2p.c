#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

int do_wait(struct mpi_request_ *req, MPI_Status *status);

// ====================================
// MPI_ send functions and auxiliaries
// ====================================
int send_remote(void* vargs) {
  struct p2p_msg *msg = (struct p2p_msg*)vargs;

  MPI_Comm comm = msg->comm;

  // get rankls for thread that's waiting for data
  struct mpi_rank_rankls *rankls = get_rankls_by_mpi_rank(shared_state, msg->dest_rank);

  // get transaction dict (given rankls)
  struct communicator_p2p_transaction_list *tl = get_p2p_transaction_list(rankls, comm, msg->src_rank);

  int error;
  error = communicator_p2p_transaction_list_append(tl, msg) ;

#if !USE_P2P_MATCH_THREAD
  bool success;
  match_message(rankls->comms[comm], &success);
#endif

  struct mpi_request_ *req = NULL;
  hpx_lco_get(msg->nicside_fut, sizeof(req), &req);

  memcpy((char*)req->buf, (char*)msg->msg_data, msg->msg_size);
  hpx_lco_set(req->fut, 0, NULL, HPX_NULL, HPX_NULL);

  return HPX_SUCCESS;
}

int mpi_isend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request_num) {
  int rank;
  mpi_comm_rank(comm, &rank);
  struct p2p_record* record = log_p2p_start(E_ISEND, rank, dest);

  struct mpi_request_ *req;
  int error =  get_new_request_num(request_num);
  if (error != SUCCESS)
    return ERROR;
  error = get_request(*request_num, &req);
  if (error != SUCCESS)
    return ERROR;
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  req->comm = rankls->comms[comm];
  req->active = true;
  static int scount = 0;
  req->fut = hpx_lco_future_new(0);
  mpi_send_(buf, &count, &datatype, &dest, &tag, &comm);
  hpx_lco_set(req->fut, 0, NULL, HPX_NULL, HPX_NULL);

  log_p2p_end(record);
  return MPI_SUCCESS_;
}

int mpi_send(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm) {
  int rank;
  mpi_comm_rank(comm, &rank);
  struct p2p_record* record = log_p2p_start(E_SEND, rank, dest);
  mpi_send_(buf, &count, &datatype, &dest, &tag, &comm);
  log_p2p_end(record);
  return 0;
}

void mpi_send_(void *buf, int *count, MPI_Datatype *fsendtype, int *dest, int *tag, MPI_Comm *fcomm) {
  MPI_Comm comm = *fcomm;
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  struct communicator_p2p_transaction_list *tl = get_p2p_transaction_list(rankls, comm, *dest);

  MPI_Datatype sendtype = *fsendtype;

  int transaction_num;
  int error;
  error = communicator_p2p_transaction_list_inc_send_count(tl, &transaction_num);
  if (error != SUCCESS)
    return;

  int rank;
  int typesize;
  mpi_comm_rank(*fcomm, &rank);
  if (error != MPI_SUCCESS_)
    return;
  mpi_type_size_(sendtype, &typesize, &error);
  if (error != MPI_SUCCESS_)
    return;

  int payload_size = sizeof(struct p2p_msg) + *count * typesize;
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, payload_size);
  hpx_parcel_set_action(p, action_send_remote);
  hpx_parcel_set_target(p, HPX_THERE(get_hpx_rank_from_mpi_rank(*dest)));
  struct p2p_msg *args = (struct p2p_msg *)hpx_parcel_get_data(p);
  args->dest_rank = *dest;
  args->src_rank = rank;
  args->tag = *tag;
  args->comm = comm;
  args->trans_num = transaction_num;
  args->msg_size = *count * typesize;
  memcpy((char*)args->msg_data, (char*)buf, *count * typesize);
  hpx_parcel_send(p, HPX_NULL);
}

// ====================================
// MPI_ recv functions
// ====================================
int mpi_recv(void *buf, int count, MPI_Datatype datatype, int src, int tag, MPI_Comm comm, MPI_Status *status) {
  int rank;
  mpi_comm_rank(comm, &rank);
  struct p2p_record* record = log_p2p_start(E_RECV, src, rank);
  MPI_Request reqnum;
  mpi_irecv(buf, count, datatype, src, tag, comm, &reqnum);

  struct mpi_request_ *req;
  int error = get_request(reqnum, &req);
  if (error != SUCCESS)
    return ERROR;
  do_wait(req, status);

  log_p2p_end(record);
  return MPI_SUCCESS_;
}

int mpi_irecv(void *buf, int count, MPI_Datatype datatype,
        int source, int tag, MPI_Comm comm, MPI_Request *request_num) {
  int rank;
  mpi_comm_rank(comm, &rank);
  struct p2p_record* record = log_p2p_start(E_IRECV, source, rank);

  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  struct mpi_request_ *req;
  int error =  get_new_request_num(request_num);
  if (error != SUCCESS)
    return ERROR;
  error = get_request(*request_num, &req);
  if (error != SUCCESS)
    return ERROR;
  req->comm = rankls->comms[comm];
  req->active = true;
  req->src = source;
  req->tag = tag;
  req->count = count;
  req->buf = buf;
  req->fut = hpx_lco_future_new(0);
  error = communicator_push_active_request(rankls->comms[comm], req);
  if (error != SUCCESS)
    return ERROR;

#if !P2P_MATCH_MESSAGE
  bool success;
  match_message(rankls->comms[comm], &success);
#endif

  log_p2p_end(record);
  return MPI_SUCCESS_;
}

#if 0
void mpi_recv_(void *buf, int *count, MPI_Datatype *datatype,
    int *source, int *tag, MPI_Comm *fcomm, int *pier) {

}
#endif

// ====================================
// MPI_ wait function
// ====================================

int do_wait(struct mpi_request_ *req, MPI_Status *status) {

#if !P2P_MATCH_MESSAGE
  bool success;
  match_message(req->comm, &success);
#endif

  int rank;
  mpi_comm_rank(MPI_COMM_WORLD_, &rank);
  if (req->active == true)
    hpx_lco_get(req->fut, 0, NULL);
  req->active = false;

  return MPI_SUCCESS_;
}

int mpi_wait(MPI_Request *request_num, MPI_Status *status) {
  if (*request_num != MPI_REQUEST_NULL) {
    struct mpi_request_ *req;
    int error = get_request(*request_num, &req);
    if (error != SUCCESS)
      return ERROR;

    int rank;
    mpi_comm_rank(MPI_COMM_WORLD_, &rank);
    struct p2p_record* record = log_p2p_start(E_WAIT, req->src, rank);

    int success = do_wait(req, status);

    log_p2p_end(record);

    return success;
  }
  else
    return MPI_SUCCESS_;
}

void mpi_waitall(int pcount,MPI_Request *request_num, MPI_Status *status) {
  int rank;
  mpi_comm_rank(MPI_COMM_WORLD_, &rank);

  struct mpi_request_ *req;
  int i;
  for (i = 0; i < pcount; i++) {
    if (request_num[i] != MPI_REQUEST_NULL) {
      int error = get_request(request_num[i], &req);

      struct p2p_record* record = log_p2p_start(E_WAITALL, req->src, rank);

#if !USE_P2P_MATCH_THREAD
      bool success;
      match_message(req->comm, &success);
#endif
      if (req->active == true)
	hpx_lco_get(req->fut, 0, NULL);
      req->active = false;

      log_p2p_end(record);
    }
  }
}
