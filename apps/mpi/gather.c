#include <hpx/hpx.h>
#include <stdio.h>
#include <string.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"


#ifdef ENABLE_DEBUG_GATHER
#define dbg_gather_printf(...)                       \
  do {                                        \
  printf(__VA_ARGS__);                        \
  fflush(stdout);                             \
  } while (0)
#else
#define dbg_gather_printf(...)
#endif


struct gather_recv_args {
  void* recv_buffer;
  int typesize;
  hpx_addr_t *futs;
};

int gather_recv(void* vargs) {
  struct op_recv_args *args = (struct op_recv_args*)vargs;

  int op = args->operation_type;
  MPI_Comm comm = args->comm;

  // get rankls for thread that's waiting for data
  struct mpi_rank_rankls *rankls = get_rankls_by_mpi_rank(shared_state, args->dest_rank);

  // get transaction dict (given rankls)
  struct communicator_transaction_dict *td = get_transaction_dict(rankls, comm, op);

  // wait until transaction is recorded on user side
  int error;

  hpx_addr_t nicside_fut;
  hpx_addr_t userside_fut; // don't actually need this for gather
  #ifdef ENABLE_DEBUG_GATHER
  hpx_thread_t *th = hpx_thread_self();
  hpx_kthread_t *kth = hpx_kthread_self();
  #endif

  //  error = communicator_transaction_dict_nicside_future(td, args->trans_num, &nicside_fut);
  error = communicator_transaction_dict_nicside_record(td, args->trans_num, &userside_fut, &nicside_fut);
  if (error != SUCCESS)
    return -1;
  dbg_gather_printf("n-- rank %d trans %d from %d about to wait on nic side future at %p in gather_recv\n", args->dest_rank,  args->trans_num, args->src_rank, (void*)nicside_fut);
  struct gather_recv_args *gather_args;
  hpx_lco_get(nicside_fut, sizeof(gather_args), (void*)&gather_args);
  dbg_gather_printf("n++ rank %d trans %d from %d done waiting on nic side future at %p in gather_recv\n", args->dest_rank, args->trans_num, args->src_rank, (void*)nicside_fut);

  // copy data to destination and trigger userside future

  memcpy((char*)gather_args->recv_buffer+args->src_rank*args->msg_size, args->msg_data, args->msg_size);
  dbg_gather_printf("ns- rank %d trans %d from %d about to set gather future at %p (%p) in gather_recv - thread = %p kthread = %p\n", args->dest_rank, args->trans_num, args->src_rank, (void*)&gather_args->futs[args->src_rank], (void*)gather_args->futs, (void*)th, (void*)kth);
  hpx_lco_set(gather_args->futs[args->src_rank], 0, NULL, HPX_NULL, HPX_NULL);
  dbg_gather_printf("ns+ rank %d trans %d from %d set gather future at %p (%p) in gather_recv\n", args->dest_rank, args->trans_num, args->src_rank, (void*)&gather_args->futs[args->src_rank], (void*)gather_args->futs);
  return HPX_SUCCESS;
}

void mpi_gather_(void *sendbuf, int *fsendcounts,
                   MPI_Datatype *fsendtype, void *recvbuf, int *recvcounts,
                   MPI_Datatype *frecvtype, int *froot, MPI_Comm *fcomm, int* pier) {
  *pier = ERROR;
  MPI_Comm comm = *fcomm;
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  struct communicator_transaction_dict *td = get_transaction_dict(rankls, comm, OP_GATHER);

  int root = *froot;
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

  if (*fsendcounts > 0) {
    int payload_size = sizeof(struct op_recv_args) + *fsendcounts * typesize;
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, payload_size);
    hpx_parcel_set_action(p, action_gather_recv);
    hpx_parcel_set_target(p, HPX_THERE(get_hpx_rank_from_mpi_rank(root)));
    struct op_recv_args *args = (struct op_recv_args *)hpx_parcel_get_data(p);
    args->src_rank = rank;
    args->dest_rank = root;
    args->trans_num = transaction_num;
    args->comm = comm;
    args->operation_type = OP_GATHER;
    args->msg_size = *fsendcounts * typesize;
    memcpy(args->msg_data, sendbuf, *fsendcounts * typesize);
    hpx_parcel_send(p, HPX_NULL);
  }

  int i;
  if (rank == root) {
    hpx_addr_t *futs = malloc(sizeof(hpx_addr_t) * size);
    for (i = 0; i < size; i++)
      futs[i] = hpx_lco_future_new(0);

    struct gather_recv_args gather_args;
    gather_args.recv_buffer = recvbuf;
    gather_args.typesize = typesize;
    gather_args.futs = futs;

    hpx_addr_t fut;
    error = communicator_transaction_dict_userside_record(td, transaction_num, &gather_args, &fut);
    if (error != SUCCESS) {
      *pier = error;
      return;
    }

    for (i = 0; i < size; i++) {
      dbg_gather_printf("u-- rank %d trans %d about to wait on gather future %d of %d at %p (%p) in mpi_gather_\n", rank, transaction_num, i, size, (void*)&futs[i], (void*)futs);
      hpx_lco_get(futs[i], 0, NULL);
      dbg_gather_printf("u++ rank %d trans %d done waiting on gather future %d of %d at %p (%p) in mpi_gather_\n", rank, transaction_num, i, size, (void*)&futs[i], (void*)futs);
    }

  }

  error =  communicator_transaction_dict_complete(td, transaction_num);
  if (error != SUCCESS) {
    *pier = error;
    return;
  }

  *pier = MPI_SUCCESS_;
  return;
}
