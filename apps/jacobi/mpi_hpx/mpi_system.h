#ifndef MPI_SYSTEM_H
#define MPI_SYSTEM_H

#include <hpx/hpx.h>
#include "list.h"
#include "mpi_wrapper.h"

#ifndef USE_P2P_MATCH_THREAD
#define USE_P2P_MATCH_THREAD 0
#endif

#define SUCCESS 0
#define ERROR -1
#define ERROR_RECORDING -2
#define ERROR_COMPLETING -2

#define DICT_CAPACITY 100
#define REQUEST_CAPACITY 1000

struct p2p_transaction_list;
struct communicator_transaction_dict;
struct communicator_op_dict comm_dict;


#ifdef ENABLE_DEBUG
#define dbg_printf(...)                       \
  do {                                        \
  printf(__VA_ARGS__);                        \
  fflush(stdout);                             \
  } while (0)
#else
#define dbg_printf(...)
#endif

struct mpi_request_ {
  struct communicator* comm;
  int src;
  int tag;
  void* buf;
  int count;
  hpx_addr_t fut;
  bool active;
};


//////////////////////////////////////////////////////////////////////////////////////////
// mpi system functions and shared state /////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

struct mpi_rank_rankls {
  bool initialized;
  int finalized;
  int rank;
  int num_comms;
  struct communicator **comms;
  struct mpi_request_* requests_backing;
  int requests_capacity;
};

/* this is the data used by the entire mpi system at this locality */
struct mpi_shared_state {
  /* global data, i.e. the same at every hpx loc */
  int size; // what MPI_Comm_size(MPI_COMM_WORLD) gives back

  /* local data, i.e. specific to this hpx loc */
  int num_ranks_here;

  int ranks_created;


  hpx_addr_t *rank_init_futs;
  int *tids;
  struct mpi_rank_rankls *rankls;
};

struct communicator {
  int rank;
  int size;
  struct communicator_op_dict *op_dict;
 // bypasses communicator_op_dict because we just have a list:
  struct communicator_p2p_transaction_list** p2p_transaction_lists;
  hpx_addr_t requests_lock;
  list_t requests;
  hpx_addr_t p2p_match_fut; // used to make sure our message matching thread is done
};

enum op_types {OP_SCATTERV,OP_GATHERV,OP_BCAST,OP_GATHER,OP_ALLTOALLV,OP_ALLTOALL,OP_REDUCE};

struct rank_init_fut_args {
  int rank;
};

struct op_recv_args {
  // for actions that implement collectives
  int src_rank;
  int dest_rank;
  MPI_Comm comm;
  int operation_type;
  int trans_num;
  int msg_size;
  char msg_data[];
};

extern struct mpi_shared_state *shared_state;

int get_hpx_rank_from_mpi_rank(int mpi_rank);

int get_rankls_index(struct mpi_shared_state *s, int tid, int* index);

struct mpi_rank_rankls* get_rankls_by_mpi_rank(struct mpi_shared_state *s, int rank);

struct mpi_rank_rankls* get_rankls(struct mpi_shared_state *s);

struct communicator_transaction_dict * get_transaction_dict(struct mpi_rank_rankls *rankls, MPI_Comm comm, int op);

struct communicator_p2p_transaction_list *get_p2p_transaction_list(struct mpi_rank_rankls *rankls, MPI_Comm comm, int dest_rank);

int resize_requests();

int get_new_request_num(int* req_num);

int get_request(int req_num, struct mpi_request_ **req);

int decide_ranks_per_node(int hpx_rank, int* num_ranks, int* num_ranks_per_node, int* min_rank_here);
int get_ranks_per_node(int hpx_rank, int* num_ranks, int* num_ranks_per_node, int* min_rank_here);

int mpi_system_init(int num_ranks, int ranks_per_node);

int mpi_system_shutdown();

void mpi_system_register_actions();

void mpi_init_(int* pier);

void mpi_comm_rank(int comm, int* rank);

void mpi_comm_rank_(int *comm, int* rank, int *pier);

void mpi_finalize_();

void mpi_comm_size_(int *comm, int* rank, int *pier);

void mpi_type_size_(MPI_Datatype datatype, int *size, int *pier);

//////////////////////////////////////////////////////////////////////////////////////////
// communicator //////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

int communicator_init(struct communicator *comm, int size, int rank);
int communicator_destroy(struct communicator *comm);
int communicator_push_active_request(struct communicator* comm, struct mpi_request_ *req);


//////////////////////////////////////////////////////////////////////////////////////////
// communicator dict /////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

struct communicator_op_dict {
  hpx_addr_t lock;
  int size;
  int capacity;
  int* ops;
  struct communicator_transaction_dict** transaction_dicts;
};

int communicator_op_dict_init(struct communicator_op_dict *d);

int communicator_op_dict_insert(struct communicator_op_dict *d, int comm, struct communicator_transaction_dict *td);

int communicator_op_dict_search(struct communicator_op_dict *d, int comm, struct communicator_transaction_dict **td);

//////////////////////////////////////////////////////////////////////////////////////////
// communicator transaction dict /////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

struct communicator_transaction_dict {
 // don't need locked but do need to be read/write atomically:
  int transaction_count_nicside;
  int transaction_count_userside;
  // we should maybe consider using padding to explicitly put these on separate cache lines

  hpx_addr_t lock;
  // All these need to be locked:
  int capacity;
  int size;
  int* trans_num;
  hpx_addr_t *nicside_fut;
  hpx_addr_t *userside_fut;
  bool* completed; 
};

int communicator_transaction_dict_init(struct communicator_transaction_dict *d);


int communicator_transaction_dict_complete(struct communicator_transaction_dict *d, int transaction_num);

int communicator_transaction_dict_userside_record(struct communicator_transaction_dict *d, int transaction_num, void* val_p, hpx_addr_t* fut);

int communicator_transaction_dict_nicside_record(struct communicator_transaction_dict *d, int transaction_num, hpx_addr_t* userside_fut, hpx_addr_t* nicside_fut);

// transaction_num [out]  - the new transaction number
int communicator_transaction_dict_inc_nicside_count(struct communicator_transaction_dict *d, int* transaction_num);

// transaction_num [out]  - the new transaction number
int communicator_transaction_dict_inc_userside_count(struct communicator_transaction_dict *d, int* transaction_num);

int communicator_transaction_dict_nicside_future(struct communicator_transaction_dict *d, int transaction_num, hpx_addr_t* fut);

  int communicator_transaction_dict_userside_future(struct communicator_transaction_dict *d, int transaction_num, hpx_addr_t* fut);


//////////////////////////////////////////////////////////////////////////////////////////
// point to point transaction dict ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

struct p2p_msg {
  // for actions that implement collectives
  int src_rank;
  int dest_rank;
  int tag;
  MPI_Comm comm;
  int trans_num;
  hpx_addr_t nicside_fut; // only used on receiving side
  int msg_size;
  char msg_data[];
};


struct communicator_p2p_transaction_list {
  struct communicator* comm;
  int src;
  int dest;

  // use atomics to access:
  int transaction_count_send;

  hpx_addr_t lock;
  // All these need to be locked:
  int curr_trans_num; // transaction number that can be matched by a *recv
  list_t received_messages;
};

int communicator_p2p_transaction_list_init(struct communicator_p2p_transaction_list *d, struct communicator* comm, int src, int dest);

int communicator_p2p_transaction_list_inc_send_count(struct communicator_p2p_transaction_list *d, int* transaction_num);

int communicator_p2p_transaction_list_append(struct communicator_p2p_transaction_list *d, struct p2p_msg* data);

int communicator_p2p_transaction_list_try_fulfill(struct communicator_p2p_transaction_list *d, struct mpi_request_* req, bool *success);

int match_message(struct communicator* comm, bool* matched);

void p2p_matching_main(void* vargs);

//
// mpi operations and associated stuff
//

hpx_action_t action_send_remote;

int send_remote(void* vargs);

void mpi_send_(void *buf, int *count, MPI_Datatype *fsendtype, int *dest, int *tag, MPI_Comm *fcomm);

int mpi_irecv(void *buf, int count, MPI_Datatype datatype,
		int source, int tag, MPI_Comm comm, MPI_Request *request);

int mpi_isend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request_num);

int mpi_wait(MPI_Request *request, MPI_Status *status);

hpx_action_t action_scatterv_recv;

int scatterv_recv(void* args); // for implementing sccaterv

void mpi_scatterv_(void *sendbuf, int *sendcounts, int *displs,
                   MPI_Datatype *sendtype, void *recvbuf, int *recvcount,
                   MPI_Datatype *recvtype, int *root, MPI_Comm *comm, int* pier);

hpx_action_t action_gatherv_recv;

int gatherv_recv(void* vargs);

void mpi_gatherv_(void *sendbuf, int *fsendcounts,
		  MPI_Datatype *fsendtype, void *recvbuf, int *recvcounts, int *displs,
		  MPI_Datatype *frecvtype, int *froot, MPI_Comm *fcomm, int* pier);

hpx_action_t action_bcast_recv;

int bcast_recv(void* vargs);

void mpi_bcast_(void *buffer, int *fdatacount, MPI_Datatype *fdatatype, 
                int *froot, MPI_Comm *fcomm, int* pier);

int mpi_allgatherv_(void *sendbuf, int *fsendcounts,
		  MPI_Datatype *fsendtype, void *recvbuf, int *recvcounts, int *displs,
		  MPI_Datatype *frecvtype, MPI_Comm *fcomm, int* pier);

hpx_action_t action_gather_recv;

int gather_recv(void* vargs);

void mpi_gather_(void *sendbuf, int *fsendcounts,
		  MPI_Datatype *fsendtype, void *recvbuf, int *recvcounts,
		  MPI_Datatype *frecvtype, int *froot, MPI_Comm *fcomm, int* pier);

hpx_action_t action_alltoallv_recv;

int alltoallv_recv(void* vargs);

void mpi_alltoallv_(void *sendbuf, int *sendcounts, int *sdispls,
                   MPI_Datatype *fsendtype, void *recvbuf, int *frecvcount,int *rdispls,
                   MPI_Datatype *frecvtype, MPI_Comm *fcomm, int* pier);

hpx_action_t action_alltoall_recv;

int alltoall_recv(void* vargs);

void mpi_alltoall_(void *sendbuf, int *fsendcounts,
                   MPI_Datatype *fsendtype, void *recvbuf, int *frecvcount,
                   MPI_Datatype *frecvtype, MPI_Comm *fcomm, int* pier); 

hpx_action_t action_reduce_recv;

int reduce_recv(void* vargs);

void mpi_reduce_(void *sendbuf, void *recvbuf, int *recvcounts,
		  MPI_Datatype *frecvtype, MPI_Op *pop, int *froot, MPI_Comm *fcomm, int* pier);
#endif
