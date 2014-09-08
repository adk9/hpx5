#include <hpx/hpx.h>
#include <stdio.h>
#include <string.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

//#define ENABLE_DEBUG_ALLTOALL

#ifdef ENABLE_DEBUG_ALLTOALL
#define dbg_alltoall_printf(...)                       \
  do {                                        \
  printf(__VA_ARGS__);                        \
  fflush(stdout);                             \
  } while (0)
#else
#define dbg_alltoall_printf(...)
#endif

struct alltoall_recv_args {
  void* recv_buffer;
  int typesize;
  hpx_addr_t *futs;
};


int alltoall_recv(void* vargs) {
  struct op_recv_args *args = (struct op_recv_args*)vargs;

  int op = args->operation_type;
  MPI_Comm comm = args->comm;

  // get rankls for thread that's waiting for data
  struct mpi_rank_rankls *rankls = get_rankls_by_mpi_rank(shared_state, args->dest_rank);

  // get transaction dict (given rankls)
  struct communicator_transaction_dict *td = get_transaction_dict(rankls, comm, op);

  // record transaction and wait until it is recorded on user side as well
  int error;

  hpx_addr_t userside_fut;
  hpx_addr_t nicside_fut;

  error =  communicator_transaction_dict_nicside_record(td, args->trans_num, &userside_fut, &nicside_fut);
  if (error != SUCCESS)
    return -1;

  // wait, copy data to destination, and trigger userside future
  struct alltoall_recv_args *alltoall_args;
  hpx_lco_get(nicside_fut, sizeof(alltoall_args), &alltoall_args);
  memcpy((char*)alltoall_args->recv_buffer + args->src_rank * args->msg_size,
                args->msg_data, args->msg_size);
  hpx_lco_set(alltoall_args->futs[args->src_rank], 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

int mpi_alltoall(void *sendbuf, int sendcounts,
                   MPI_Datatype sendtype, void *recvbuf, int recvcount,
                   MPI_Datatype recvtype, MPI_Comm comm) {
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  struct communicator_transaction_dict *td = get_transaction_dict(rankls, comm, OP_ALLTOALL);

  int transaction_num;
  int error;
  error = communicator_transaction_dict_inc_userside_count(td, &transaction_num);
  if (error != SUCCESS)
    return error;

  int rank;
  int size;
  int typesize;
  mpi_comm_rank_(&comm, &rank, &error);
  if (error != MPI_SUCCESS_)
    return error;
  mpi_comm_size_(&comm, &size, &error);
  if (error != MPI_SUCCESS_)
    return error;
  mpi_type_size_(recvtype, &typesize, &error); // recvtype since that is significant at all ranks and sendtype is not
  if (error != MPI_SUCCESS_)
    return error;

  int i;
  for (i = 0; i < size; i++) {
    if (sendcounts > 0) {
      int payload_size = sizeof(struct op_recv_args) + sendcounts * typesize;
      hpx_parcel_t *p = hpx_parcel_acquire(NULL, payload_size);
      hpx_parcel_set_action(p, action_alltoall_recv);
      hpx_parcel_set_target(p, HPX_THERE(get_hpx_rank_from_mpi_rank(i)));
      struct op_recv_args *args = hpx_parcel_get_data(p);
      args->src_rank = rank;
      args->dest_rank = i;
      args->trans_num = transaction_num;
      args->comm = comm;
      args->operation_type = OP_ALLTOALL;
      args->msg_size = sendcounts * typesize;
      memcpy((char*)args->msg_data, (char*)sendbuf + i * args->msg_size, args->msg_size);
      hpx_parcel_send(p, HPX_NULL);
    }
  } // end for(i)

  hpx_addr_t *futs = malloc(sizeof(hpx_addr_t) * size);
  for (i = 0; i < size; i++)
    futs[i] = hpx_lco_future_new(0);

  struct alltoall_recv_args alltoall_args;
  alltoall_args.recv_buffer = recvbuf;
  alltoall_args.typesize = typesize;
  alltoall_args.futs = futs;

  hpx_addr_t fut;
  error = communicator_transaction_dict_userside_record(td, transaction_num, &alltoall_args, &fut);
  if (error != SUCCESS) {
    return error;
  }

  for (i = 0; i < size; i++) {
    hpx_lco_get(futs[i], 0, NULL);
  }

  error =  communicator_transaction_dict_complete(td, transaction_num);
  if (error != SUCCESS) {
    return error;
  }

  return MPI_SUCCESS_;
}
