#include <hpx/hpx.h>
#include <stdio.h>
#include <string.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

//#define ENABLE_DEBUG_SCATTERV

#ifdef ENABLE_DEBUG_SCATTERV
#define dbg_scatterv_printf(...)                       \
  do {                                        \
  printf(__VA_ARGS__);                        \
  fflush(stdout);                             \
  } while (0)
#else
#define dbg_scatterv_printf(...)
#endif


// scatterv_recv {{{
int scatterv_recv(void* vargs) {
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
  dbg_scatterv_printf("nrr rank %d trans %d recording nic side in scatterv_recv - thread = %p kthread = %p\n", args->rank, transaction_num,  (void*)th, (void*)kth);
  error =  communicator_transaction_dict_nicside_record(td, transaction_num, &userside_fut, &nicside_fut);

  dbg_scatterv_printf("n-- rank %d trnas %d about to wait on nic side future at %p in scatterv_recv\n", args->rank, transaction_num, (void*)nicside_fut);
  void* datap;
  hpx_lco_get(nicside_fut, (void*)&datap, sizeof(datap));
  dbg_scatterv_printf("n++ rank %d trans %d done waiting on nic side future at %p in scatterv_recv\n", args->rank, transaction_num, (void*)nicside_fut);

  // copy data to destination and trigger userside future
  memcpy((char*)datap, args->msg_data, args->msg_size);
  dbg_scatterv_printf("ns- rank %d trans %d about to set user side future in scatterv_recv at %p - thread = %p kthread = %p\n", args->rank, transaction_num, (void*)userside_fut, (void*)th, (void*)kth);
  hpx_lco_set(userside_fut, NULL, 0, HPX_NULL);
  dbg_scatterv_printf("ns+ rank %d trans %d set user side future in scatterv_recv\n", args->rank, transaction_num);
  return HPX_SUCCESS;
}
// }}}

// scatterv {{{
void mpi_scatterv_(void *sendbuf, int *sendcounts, int *displs,
                   MPI_Datatype *fsendtype, void *recvbuf, int *frecvcount,
                   MPI_Datatype *frecvtype, int *froot, MPI_Comm *fcomm, int* pier) {
  *pier = ERROR;
  MPI_Comm comm = *fcomm;
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  struct communicator_transaction_dict *td = get_transaction_dict(rankls, comm, OP_SCATTERV);

  int root = *froot;
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

  if (root == rank) {
    for (int i = 0; i < size; i++) {
      //if (i == root)
      //        memcpy(recvbuf, sendbuf + displs[i] * typesize, sendcounts[i] * typesize);
      //      else {
      if (sendcounts[i] > 0) {
          int payload_size = sizeof(struct op_recv_args) + sendcounts[i] * typesize;
          hpx_parcel_t *p = hpx_parcel_acquire(payload_size);
          hpx_parcel_set_action(p, action_scatterv_recv);
          hpx_parcel_set_target(p, HPX_THERE(get_hpx_rank_from_mpi_rank(i)));
          struct op_recv_args *args = (struct op_recv_args *)hpx_parcel_get_data(p);
          args->dest_rank = i;
          args->comm = comm;
          args->operation_type = OP_SCATTERV;
          args->msg_size = sendcounts[i] * typesize;
          memcpy(args->msg_data, (char*)sendbuf + displs[i] * typesize, sendcounts[i] * typesize);
          hpx_parcel_send(p);
          //        }
      }
    } // end for(i)
  } // end if(root==rank)

  hpx_addr_t fut;
  dbg_scatterv_printf("urr rank %d trans %d recording user side in mpi_scatterv_\n", rank, transaction_num);
  fflush(stdout);
  error = communicator_transaction_dict_userside_record(td, transaction_num, recvbuf, &fut);
  if (error != SUCCESS) {
    *pier = error;
    return;
  }

  dbg_scatterv_printf("u-- rank %d trans %d about to wait on user side future at %p in mpi_scatterv_\n", rank, transaction_num, (void*)fut);
  hpx_lco_get(fut, NULL, 0);
  dbg_scatterv_printf("u++ rank %d trans %d done waiting on user side future at %p in mpi_scatterv_\n", rank, transaction_num, (void*)fut);

  error =  communicator_transaction_dict_complete(td, transaction_num);
  if (error != SUCCESS) {
    *pier = error;
    return;
  }

  *pier = MPI_SUCCESS_;
  return;
}
// }}}
