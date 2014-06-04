#include <hpx/hpx.h>
#include <stdio.h>
#include <string.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"


#ifdef ENABLE_DEBUG_REDUCE
#define dbg_reduce_printf(...)                       \
  do {                                        \
  printf(__VA_ARGS__);                        \
  fflush(stdout);                             \
  } while (0)
#else
#define dbg_reduce_printf(...)
#endif


struct reduce_recv_args {
  void* recv_buffer;
  int typesize;
  MPI_Datatype type;
  MPI_Op op;
  hpx_addr_t *futs;
};
#include <unistd.h>
int reduce_recv(void* vargs) {
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
  hpx_addr_t userside_fut; // don't actually need this for reduce
  #ifdef ENABLE_DEBUG_REDUCE
  hpx_thread_t *th = hpx_thread_self();
  hpx_kthread_t *kth = hpx_kthread_self();
  #endif

  //  error = communicator_transaction_dict_nicside_future(td, args->trans_num, &nicside_fut);
  error = communicator_transaction_dict_nicside_record(td, args->trans_num, &userside_fut, &nicside_fut);
  if (error != SUCCESS)
    return -1;
  dbg_reduce_printf("n-- rank %d trans %d from %d about to wait on nic side future at %p in reduce_recv\n", args->dest_rank,  args->trans_num, args->src_rank, (void*)nicside_fut);
  struct reduce_recv_args *reduce_args = NULL;
  hpx_lco_get(nicside_fut, (void*)&reduce_args, sizeof(reduce_args));
  dbg_reduce_printf("n++ rank %d trans %d from %d done waiting on nic side future at %p in reduce_recv\n", args->dest_rank, args->trans_num, args->src_rank, (void*)nicside_fut);

  // copy data to destination and trigger userside future
  //memcpy((char*)reduce_args->recv_buffer+args->src_rank*args->msg_size, args->msg_data, args->msg_size);
  if ( reduce_args->op == MPI_MAX ) {
    if (reduce_args->type == MPI_INTEGER) {
      int *result = (int *) reduce_args->recv_buffer;
      int *incoming = (int *) args->msg_data;
      for (int i=0;i<args->msg_size/reduce_args->typesize;i++) {
        if ( result[i] < incoming[i] ) {
          result[i] = incoming[i];
        }
      }
    } else if (reduce_args->type == MPI_DOUBLE_PRECISION || reduce_args->type == MPI_DOUBLE) {
      double *result = (double *) reduce_args->recv_buffer;
      double *incoming = (double *) args->msg_data;
      for (int i=0;i<args->msg_size/reduce_args->typesize;i++) {
        if ( result[i] < incoming[i] ) {
          result[i] = incoming[i];
        }
      }
    } else {
      printf(" No Op %d support for type %d\n",reduce_args->op,reduce_args->type);
      return -1;
    }
  } else if ( reduce_args->op == MPI_MIN ) {
    if (reduce_args->type == MPI_INTEGER) {
      int *result = (int *) reduce_args->recv_buffer;
      int *incoming = (int *) args->msg_data;
      for (int i=0;i<args->msg_size/reduce_args->typesize;i++) {
        if ( result[i] > incoming[i] ) {
          result[i] = incoming[i];
        }
      }
    } else if (reduce_args->type == MPI_DOUBLE_PRECISION || reduce_args->type == MPI_DOUBLE) {
      double *result = (double *) reduce_args->recv_buffer;
      double *incoming = (double *) args->msg_data;

      for (int i=0;i<args->msg_size/reduce_args->typesize;i++) {
        if ( result[i] > incoming[i] ) {
          result[i] = incoming[i];
        }
      }
    } else {
      printf(" No Op %d support for type %d\n",reduce_args->op,reduce_args->type);
      return -1;
    }
  } else if ( reduce_args->op == MPI_SUM ) {
    if (reduce_args->type == MPI_INTEGER) {
      int *result = (int *) reduce_args->recv_buffer;
      int *incoming = (int *) args->msg_data;
      for (int i=0;i<args->msg_size/reduce_args->typesize;i++) {
        result[i] += incoming[i];
      }
    } else if (reduce_args->type == MPI_DOUBLE_PRECISION || reduce_args->type == MPI_DOUBLE) {
      double *result = (double *) reduce_args->recv_buffer;
      double *incoming = (double *) args->msg_data;
      for (int i=0;i<args->msg_size/reduce_args->typesize;i++) {
        result[i] += incoming[i];
      }
    } else {
      printf(" No Op %d support for type %d\n",reduce_args->op,reduce_args->type);
      return -1;
    }
  } else if ( reduce_args->op == MPI_LOR ) {
    if (reduce_args->type == MPI_LOGICAL) {
      int *result = (int *) reduce_args->recv_buffer;
      int *incoming = (int *) args->msg_data;
      for (int i=0;i<args->msg_size/reduce_args->typesize;i++) {
        if ( incoming[i] == 1 ) {
          result[i] = 1;
        }
      }
    } else {
      printf(" No Op %d support for type %d\n",reduce_args->op,reduce_args->type);
      return -1;
    }
  } else if ( reduce_args->op == MPI_MINLOC ) {
    if (reduce_args->type == MPI_2DOUBLE_PRECISION) {
      double *result = (double *) reduce_args->recv_buffer;
      double *incoming = (double *) args->msg_data;
      for (int i=0;i<args->msg_size/reduce_args->typesize;i=i+2) {
        if ( result[i] < incoming[i] ) {
          result[i] = incoming[i];
          result[i+1] = incoming[i+1];
        }
      }
    } else {
      printf(" No Op %d support for type %d\n",reduce_args->op,reduce_args->type);
      return -1;
    }
  } else {
    printf(" No Op %d support\n",reduce_args->op);
    return -1;
  }
  dbg_reduce_printf("ns- rank %d trans %d from %d about to set reduce future at %p (%p) in reduce_recv - thread = %p kthread = %p\n", args->dest_rank, args->trans_num, args->src_rank, (void*)&reduce_args->futs[args->src_rank], (void*)reduce_args->futs, (void*)th, (void*)kth);
  hpx_lco_set(reduce_args->futs[args->src_rank], NULL, 0, HPX_NULL);
  dbg_reduce_printf("ns+ rank %d trans %d from %d set reduce future at %p (%p) in reduce_recv\n", args->dest_rank, args->trans_num, args->src_rank, (void*)&reduce_args->futs[args->src_rank], (void*)reduce_args->futs);
  return HPX_SUCCESS;
}

void mpi_reduce_(void *sendbuf, void *recvbuf, int *recvcounts,
                   MPI_Datatype *frecvtype, MPI_Op *pop, int *froot, MPI_Comm *fcomm, int* pier) {
  *pier = ERROR;
  MPI_Comm comm = *fcomm;
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  struct communicator_transaction_dict *td = get_transaction_dict(rankls, comm, OP_REDUCE);

  int root = *froot;
  MPI_Datatype recvtype = *frecvtype;

  int transaction_num;
  int error;
  error = communicator_transaction_dict_inc_userside_count(td, &transaction_num);
  if (error != SUCCESS)
    return;

  int rank;
  int size;
  int typesize;
  mpi_comm_rank(*fcomm, &rank);
  if (error != MPI_SUCCESS_)
    return;
  mpi_comm_size_(&comm, &size, &error);
  if (error != MPI_SUCCESS_)
    return;
  mpi_type_size_(recvtype, &typesize, &error); // recvtype since that is significant at all ranks and sendtype is not
  if (error != MPI_SUCCESS_)
    return;

  if (*recvcounts > 0) {
    int payload_size = sizeof(struct op_recv_args) + *recvcounts * typesize;
    hpx_parcel_t *p = hpx_parcel_acquire(payload_size);
    hpx_parcel_set_action(p, action_reduce_recv);
    hpx_parcel_set_target(p, HPX_THERE(get_hpx_rank_from_mpi_rank(root)));
    struct op_recv_args *args = (struct op_recv_args *)hpx_parcel_get_data(p);
    args->src_rank = rank;
    args->dest_rank = root;
    args->trans_num = transaction_num;
    args->comm = comm;
    args->operation_type = OP_REDUCE;
    args->msg_size = *recvcounts * typesize;
    //double *res = (double *) sendbuf;
    memcpy(args->msg_data, sendbuf, *recvcounts * typesize);
    hpx_parcel_send(p);
  }

  if (rank == root) {
    MPI_Op op = *pop;
    MPI_Datatype type = *frecvtype;
    int count = *recvcounts;
    // initialize the recvbuffer
    if ( op == MPI_MAX ) {
      if (type == MPI_INTEGER) {
        int *result = (int *) recvbuf;
        int *incoming = (int *) sendbuf;
        for (int i=0;i<count;i++) {
          result[i] = incoming[i];
        }
      } else if (type == MPI_DOUBLE_PRECISION || type == MPI_DOUBLE) {
        double *result = (double *) recvbuf;
        double *incoming = (double *) sendbuf;
        for (int i=0;i<count;i++) {
          result[i] = incoming[i];
        }
      } else {
        printf(" No Op %d support for type %d\n",op,type);
        return;
      }
    } else if ( op == MPI_MIN ) {
      if (type == MPI_INTEGER) {
        int *result = (int *) recvbuf;
        int *incoming = (int *) sendbuf;
        for (int i=0;i<count;i++) {
          result[i] = incoming[i];
        }
      } else if (type == MPI_DOUBLE_PRECISION || type == MPI_DOUBLE) {
        double *result = (double *) recvbuf;
        double *incoming = (double *) sendbuf;
        for (int i=0;i<count;i++) {
          result[i] = incoming[i];
        }
      } else {
        printf(" No Op %d support for type %d\n",op,type);
        return;
      }
    } else if ( op == MPI_SUM ) {
      if (type == MPI_INTEGER) {
        int *result = (int *) recvbuf;
        for (int i=0;i<count;i++) {
          result[i] = 0;
        }
      } else if (type == MPI_DOUBLE_PRECISION || type == MPI_DOUBLE) {
        double *result = (double *) recvbuf;
        for (int i=0;i<count;i++) {
          result[i] = 0.0;
        }
      } else {
        printf(" No Op %d support for type %d\n",op,type);
        return;
      }
    } else if ( op == MPI_LOR ) {
      if (type == MPI_LOGICAL) {
        int *result = (int *) recvbuf;
        int *incoming = (int *) sendbuf;
        for (int i=0;i<count;i++) {
          result[i] = incoming[i];
        }
      } else {
        printf(" No Op %d support for type %d\n",op,type);
        return;
      }
    } else if ( op == MPI_MINLOC ) {
      if (type == MPI_2DOUBLE_PRECISION) {
        double *result = (double *) recvbuf;
        double *incoming = (double *) sendbuf;
        for (int i=0;i<count;i=i+2) {
          result[i] = incoming[i];
          result[i+1] = incoming[i+1];
        }
      } else {
        printf(" No Op %d support for type %d\n",op,type);
        return;
      }
    } else {
      printf(" No Op %d support\n",op);
      return;
    }

    hpx_addr_t *futs = malloc(sizeof(hpx_addr_t) * size);
    for (int i = 0; i < size; i++)
      futs[i] = hpx_lco_future_new(0);

    struct reduce_recv_args reduce_args;
    reduce_args.recv_buffer = recvbuf;
    reduce_args.typesize = typesize;
    reduce_args.type = *frecvtype;
    reduce_args.op = *pop;
    reduce_args.futs = futs;

    hpx_addr_t fut;
    error = communicator_transaction_dict_userside_record(td, transaction_num, &reduce_args, &fut);
    if (error != SUCCESS) {
      *pier = error;
      return;
    }

    for (int i = 0; i < size; i++) {
      dbg_reduce_printf("u-- rank %d trans %d about to wait on reduce future %d of %d at %p (%p) in mpi_reduce_\n", rank, transaction_num, i, size, (void*)&futs[i], (void*)futs);
      hpx_lco_get(futs[i], NULL, 0);
      dbg_reduce_printf("u++ rank %d trans %d done waiting on reduce future %d of %d at %p (%p) in mpi_reduce_\n", rank, transaction_num, i, size, (void*)&futs[i], (void*)futs);
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

void mpi_reduce(void *sendbuf, void *recvbuf, int recvcounts,
                   MPI_Datatype frecvtype, MPI_Op pop, int froot, MPI_Comm fcomm) {
   int err;
   mpi_reduce_(sendbuf, recvbuf, &recvcounts,
                   &frecvtype, &pop, &froot, &fcomm, &err);
}
