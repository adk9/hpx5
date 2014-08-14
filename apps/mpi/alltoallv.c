#include <hpx/hpx.h>
#include <stdio.h>
#include <string.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

//#define ENABLE_DEBUG_ALLTOALLV

#ifdef ENABLE_DEBUG_ALLTOALLV
#define dbg_alltoallv_printf(...)                       \
  do {                                        \
  printf(__VA_ARGS__);                        \
  fflush(stdout);                             \
  } while (0)
#else
#define dbg_alltoallv_printf(...)
#endif

struct alltoallv_recv_args {
  void* recv_buffer;
  int* displs;
  int typesize;
  hpx_addr_t* futs;
};


int alltoallv_recv(void* vargs) {
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

  // copy data to destination and trigger userside future
  struct alltoallv_recv_args *alltoallv_args;
  hpx_lco_get(nicside_fut, sizeof(alltoallv_args), &alltoallv_args);
  memcpy((char*)alltoallv_args->recv_buffer + alltoallv_args->displs[args->src_rank] * alltoallv_args->typesize,
                args->msg_data, args->msg_size);
  hpx_lco_set(alltoallv_args->futs[args->src_rank], 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

void mpi_alltoallv_(void *sendbuf, int *sendcounts, int *sdispls,
                   MPI_Datatype *fsendtype, void *recvbuf, int *frecvcount,int *rdispls,
                   MPI_Datatype *frecvtype, MPI_Comm *fcomm, int* pier) {
  *pier = ERROR;
  MPI_Comm comm = *fcomm;
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  struct communicator_transaction_dict *td = get_transaction_dict(rankls, comm, OP_ALLTOALLV);

  int recvcount = *frecvcount;
  MPI_Datatype sendtype = *fsendtype;
  MPI_Datatype recvtype = *frecvtype;

  int transaction_num;
  int error;
  error = communicator_transaction_dict_inc_userside_count(td, &transaction_num);
  if (error != SUCCESS)
    return;

  int rank;
  int size;
  int typesize;
  mpi_comm_rank_(&comm, &rank, &error);
  if (error != MPI_SUCCESS_)
    return;
  mpi_comm_size_(&comm, &size, &error);
  if (error != MPI_SUCCESS_)
    return;
  mpi_type_size_(recvtype, &typesize, &error); // recvtype since that is significant at all ranks and sendtype is not
  if (error != MPI_SUCCESS_)
    return;

  int i;
  for (i = 0; i < size; i++) {
    if (sendcounts[i] > 0) {
      int payload_size = sizeof(struct op_recv_args) + sendcounts[i] * typesize;
      hpx_parcel_t *p = hpx_parcel_acquire(NULL, payload_size);
      hpx_parcel_set_action(p, action_alltoallv_recv);
      hpx_parcel_set_target(p, HPX_THERE(get_hpx_rank_from_mpi_rank(i)));
      struct op_recv_args *args = (struct op_recv_args *)hpx_parcel_get_data(p);
      args->src_rank = rank;
      args->dest_rank = i;
      args->trans_num = transaction_num;
      args->comm = comm;
      args->operation_type = OP_ALLTOALLV;
      args->msg_size = sendcounts[i] * typesize;
      memcpy((char*)args->msg_data, (char*)sendbuf + sdispls[i] * typesize, sendcounts[i] * typesize);
      hpx_parcel_send(p, HPX_NULL);
    }
  } // end for(i)

  hpx_addr_t *futs = malloc(sizeof(hpx_addr_t) * size);
  for (i = 0; i < size; i++)
    futs[i] = hpx_lco_future_new(0);

  struct alltoallv_recv_args alltoallv_args;
  alltoallv_args.recv_buffer = recvbuf;
  alltoallv_args.displs = rdispls;
  alltoallv_args.typesize = typesize;
  alltoallv_args.futs = futs;

  hpx_addr_t fut;
  error = communicator_transaction_dict_userside_record(td, transaction_num, &alltoallv_args, &fut);
  if (error != SUCCESS) {
    *pier = error;
    return;
  }

  for (i = 0; i < size; i++) {
    hpx_lco_get(futs[i], 0, NULL);
  }

  error =  communicator_transaction_dict_complete(td, transaction_num);
  if (error != SUCCESS) {
    *pier = error;
    return;
  }

  *pier = MPI_SUCCESS_;
  return;
}
