#include <hpx/hpx.h>
#include <stdio.h>
#include <string.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

#ifdef ENABLE_DEBUG_BCAST
#define dbg_bcast_printf(...)                       \
  do {                                        \
  printf(__VA_ARGS__);                        \
  fflush(stdout);                             \
  } while (0)
#else
#define dbg_bcast_printf(...)
#endif


int bcast_recv(void* vargs) {
  struct op_recv_args *args = (struct op_recv_args*)vargs;

  int op = args->operation_type;
  MPI_Comm comm = args->comm;

  // get rankls for thread that's waiting for data
  struct mpi_rank_rankls *rankls = get_rankls_by_mpi_rank(shared_state, args->dest_rank);

  // get transaction dict (given rankls)
  struct communicator_transaction_dict *td = get_transaction_dict(rankls, comm, op);

  // record transaction and wait until it is recorded on user side as well
  int transaction_num;
  int error;
  error = communicator_transaction_dict_inc_nicside_count(td, &transaction_num);
  if (error != SUCCESS)
    return -1;

  hpx_addr_t userside_fut;
  hpx_addr_t nicside_fut;
  #ifdef ENABLE_DEBUG_SCATTERV
  hpx_thread_t *th = hpx_thread_self();
  hpx_kthread_t *kth = hpx_kthread_self();
  #endif
  dbg_bcast_printf("nrr rank %d trans %d recording nic side in scatterv_recv - thread = %p kthread = %p\n", args->rank, transaction_num,  (void*)th, (void*)kth);
  error =  communicator_transaction_dict_nicside_record(td, transaction_num, &userside_fut, &nicside_fut);

  dbg_bcast_printf("n-- rank %d trnas %d about to wait on nic side future at %p in bcast_recv\n", args->rank, transaction_num, (void*)nicside_fut);
  void* datap;
  hpx_lco_get(nicside_fut, sizeof(datap), &datap);
  dbg_bcast_printf("n++ rank %d trans %d done waiting on nic side future at %p in bcast_recv\n", args->rank, transaction_num, (void*)nicside_fut);

  // copy data to destination and trigger userside future
  memcpy((char*)datap, args->msg_data, args->msg_size);
  dbg_bcast_printf("ns- rank %d trans %d about to set user side future in bcast_recv at %p - thread = %p kthread = %p\n", args->rank, transaction_num, (void*)userside_fut, (void*)th, (void*)kth);
  hpx_lco_set(userside_fut, 0, NULL, HPX_NULL, HPX_NULL);
  dbg_bcast_printf("ns+ rank %d trans %d set user side future in bcast_recv\n", args->rank, transaction_num);
  return HPX_SUCCESS;
}

void mpi_bcast_(void *buffer, int *fdatacount, MPI_Datatype *fdatatype,
                int *froot, MPI_Comm *fcomm, int* pier) {
  *pier = ERROR;
  MPI_Comm comm = *fcomm;

  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  struct communicator_transaction_dict *td = get_transaction_dict(rankls, comm, OP_BCAST);

  int root = *froot;
  int datacount = *fdatacount;
  MPI_Datatype datatype = *fdatatype;

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
  mpi_type_size_(datatype, &typesize, &error); // recvtype since that is significant at all ranks and sendtype is not
  if (error != MPI_SUCCESS_)
    return;

  int i;
  if (root == rank) {
      for (i = 0; i < size; i++) {
          if (datacount > 0) {
              int payload_size = sizeof(struct op_recv_args) + datacount * typesize;
              hpx_parcel_t *p = hpx_parcel_acquire(NULL, payload_size);
              hpx_parcel_set_action(p, action_bcast_recv);
              hpx_parcel_set_target(p, HPX_THERE(get_hpx_rank_from_mpi_rank(i)));
              struct op_recv_args *args = (struct op_recv_args *)hpx_parcel_get_data(p);
              args->dest_rank = i;
              args->comm = comm;
              args->operation_type = OP_BCAST;
              args->msg_size = datacount * typesize;
              memcpy(args->msg_data, buffer, datacount * typesize);
              hpx_parcel_send(p, HPX_NULL);
          }
      } // end for(i)
  } // end if(root==rank)

  hpx_addr_t fut;
  dbg_bcast_printf("urr rank %d trans %d recording user side in mpi_bcast_\n", rank, transaction_num);
  fflush(stdout);
  error = communicator_transaction_dict_userside_record(td, transaction_num, buffer, &fut);
  if (error != SUCCESS) {
    *pier = error;
    return;
  }

  dbg_bcast_printf("u-- rank %d trans %d about to wait on user side future at %p in mpi_bcast_\n", rank, transaction_num, (void*)fut);
  hpx_lco_get(fut, 0, NULL);
  dbg_bcast_printf("u++ rank %d trans %d done waiting on user side future at %p in mpi_bcast_\n", rank, transaction_num, (void*)fut);

  error =  communicator_transaction_dict_complete(td, transaction_num);
  if (error != SUCCESS) {
    *pier = error;
    return;
  }

  *pier = MPI_SUCCESS_;
  return;
}
