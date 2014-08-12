#include <hpx/hpx.h>
#include <stdio.h>
#include <string.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

//#define ENABLE_DEBUG_GATHERV

#ifdef ENABLE_DEBUG_GATHERV
#define dbg_gatherv_printf(...)                       \
  do {                                        \
  printf(__VA_ARGS__);                        \
  fflush(stdout);                             \
  } while (0)
#else
#define dbg_gatherv_printf(...)
#endif

// gatherv_recv {{{

struct gatherv_recv_args {
  void* recv_buffer;
  int* displs;
  int typesize;
  hpx_addr_t *futs;
};

int gatherv_recv(void* vargs) {
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
  hpx_addr_t userside_fut; // don't actually need this for gatherv
  #ifdef ENABLE_DEBUG_GATHERV
  hpx_thread_t *th = hpx_thread_self();
  hpx_kthread_t *kth = hpx_kthread_self();
  #endif

  //  error = communicator_transaction_dict_nicside_future(td, args->trans_num, &nicside_fut);
  error = communicator_transaction_dict_nicside_record(td, args->trans_num, &userside_fut, &nicside_fut);
  if (error != SUCCESS)
    return -1;
  dbg_gatherv_printf("n-- rank %d trans %d from %d about to wait on nic side future at %p in gatherv_recv\n", args->dest_rank,  args->trans_num, args->src_rank, (void*)nicside_fut);
  struct gatherv_recv_args *gatherv_args;
  hpx_lco_get(nicside_fut, sizeof(gatherv_args), (void*)&gatherv_args)
  dbg_gatherv_printf("n++ rank %d trans %d from %d done waiting on nic side future at %p in gatherv_recv\n", args->dest_rank, args->trans_num, args->src_rank, (void*)nicside_fut);

  // copy data to destination and trigger userside future

  memcpy((char*)gatherv_args->recv_buffer + gatherv_args->displs[args->src_rank] * gatherv_args->typesize,
                           args->msg_data, args->msg_size);
  dbg_gatherv_printf("ns- rank %d trans %d from %d about to set gatherv future at %p (%p) in gatherv_recv - thread = %p kthread = %p\n", args->dest_rank, args->trans_num, args->src_rank, (void*)&gatherv_args->futs[args->src_rank], (void*)gatherv_args->futs, (void*)th, (void*)kth);
  hpx_lco_set(gatherv_args->futs[args->src_rank], 0, NULL, HPX_NULL, HPX_NULL);
  dbg_gatherv_printf("ns+ rank %d trans %d from %d set gatherv future at %p (%p) in gatherv_recv\n", args->dest_rank, args->trans_num, args->src_rank, (void*)&gatherv_args->futs[args->src_rank], (void*)gatherv_args->futs);
  return HPX_SUCCESS;
}
// }}}

// gatherv {{{
void mpi_gatherv_(void *sendbuf, int *fsendcounts,
                   MPI_Datatype *fsendtype, void *recvbuf, int *recvcounts, int *displs,
                   MPI_Datatype *frecvtype, int *froot, MPI_Comm *fcomm, int* pier) {
  *pier = ERROR;
  MPI_Comm comm = *fcomm;
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  struct communicator_transaction_dict *td = get_transaction_dict(rankls, comm, OP_GATHERV);

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
    hpx_parcel_set_action(p, action_gatherv_recv);
    hpx_parcel_set_target(p, HPX_THERE(get_hpx_rank_from_mpi_rank(root)));
    struct op_recv_args *args = (struct op_recv_args *)hpx_parcel_get_data(p);
    args->src_rank = rank;
    args->dest_rank = root;
    args->trans_num = transaction_num;
    args->comm = comm;
    args->operation_type = OP_GATHERV;
    args->msg_size = *fsendcounts * typesize;
    memcpy(args->msg_data, sendbuf, *fsendcounts * typesize);
    hpx_parcel_send(p, HPX_NULL);
  }

  if (rank == root) {
    hpx_addr_t *futs = malloc(sizeof(hpx_addr_t) * size);
    for (int i = 0; i < size; i++)
      futs[i] = hpx_lco_future_new(0);

    struct gatherv_recv_args gatherv_args;
    gatherv_args.recv_buffer = recvbuf;
    gatherv_args.displs = displs;
    gatherv_args.typesize = typesize;
    gatherv_args.futs = futs;

    hpx_addr_t fut;
    error = communicator_transaction_dict_userside_record(td, transaction_num, &gatherv_args, &fut);
    if (error != SUCCESS) {
      *pier = error;
      return;
    }

    for (int i = 0; i < size; i++) {
      dbg_gatherv_printf("u-- rank %d trans %d about to wait on gatherv future %d of %d at %p (%p) in mpi_gatherv_\n", rank, transaction_num, i, size, (void*)&futs[i], (void*)futs);
      hpx_lco_get(futs[i], 0, NULL);
      dbg_gatherv_printf("u++ rank %d trans %d done waiting on gatherv future %d of %d at %p (%p) in mpi_gatherv_\n", rank, transaction_num, i, size, (void*)&futs[i], (void*)futs);
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
// }}}
