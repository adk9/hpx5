#include <assert.h>
#include <stdio.h>
#include <string.h> // for memset()

#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "mpi_wrapper.h"
#include "mpi_system.h"
#include "list.h"
#include "logging.h"

void mpi_bcast_(void *buffer, int *fdatacount, MPI_Datatype *fdatatype,
        int *froot, MPI_Comm *fcomm, int* pier); // TODO: replace with barrier or an internal barrier

//////////////////////////////////////////////////////////////////////////////////////////
// mpi system functions and shared state /////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

struct init_args {
  int num_ranks;
  int num_ranks_per;
};

static hpx_action_t action_init = 0;
static hpx_action_t action_shutdown = 0;
static hpx_action_t action_mark_rank_inited = 0;

struct mpi_shared_state *shared_state = NULL;

struct mpi_rank_rankls* get_rankls_by_mpi_rank(struct mpi_shared_state *s, int rank) {
  int index = rank / hpx_get_num_ranks();
  struct mpi_rank_rankls* rankls = &s->rankls[index];
  return rankls;
}

// to change the distribution of ranks to locs, change this, get_mpi_rank (and init_action and mpi_system_init ?)
int get_hpx_rank_from_mpi_rank(int mpi_rank) {
  return mpi_rank % hpx_get_num_ranks();
}

int get_rankls_index(struct mpi_shared_state *s, int tid, int* index) {
  int i = 0;
  for (i = 0; i < s->num_ranks_here; i++)
    if (s->tids[i] == tid)
      break;
  if (i == s->num_ranks_here)
    return ERROR;
  *index = i;
  return SUCCESS;
}

struct mpi_rank_rankls *get_rankls(struct mpi_shared_state *s) {
  int index = 0;
  get_rankls_index(s, hpx_thread_get_tls_id(), &index);
  return &s->rankls[index];
}

struct communicator_transaction_dict * get_transaction_dict(struct mpi_rank_rankls *rankls, MPI_Comm comm, int op) {
  struct communicator_op_dict *comm_op_dict = rankls->comms[comm]->op_dict;
  struct communicator_transaction_dict *td;
  int error = communicator_op_dict_search(comm_op_dict, op, &td);
  if (error != SUCCESS)
    return NULL;
  return td;
}

struct communicator_p2p_transaction_list *get_p2p_transaction_list(struct mpi_rank_rankls *rankls, MPI_Comm comm, int dest_rank) {
  return rankls->comms[comm]->p2p_transaction_lists[dest_rank];
}

int resize_requests() {
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  int old_capacity = rankls->requests_capacity;
  rankls->requests_capacity *= 2;
  rankls->requests_backing = realloc(rankls->requests_backing, rankls->requests_capacity);

  // todo error handling
  memset(&rankls->requests_backing[old_capacity], 0, rankls->requests_capacity - old_capacity);

  // LD: what's the point of this if we just did it with the memset?
  for (int i = old_capacity; i < rankls->requests_capacity; i++)
    rankls->requests_backing[i].active = false;

  return SUCCESS;
}

int get_new_request_num(int* req_num) {
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);

  // 0 is a null request!
  for (int i = 1; i < rankls->requests_capacity; ++i) {
    if (rankls->requests_backing[i].active == false) {
      rankls->requests_backing[i].active = true;
      *req_num = i;
      return SUCCESS;
    }
  }

  *req_num = rankls->requests_capacity;
  return resize_requests();
}

int get_request(int req_num, struct mpi_request_** req) {
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  if (req_num > rankls->requests_capacity)
    return ERROR;
  *req = &rankls->requests_backing[req_num];
  return SUCCESS;
}

// set one parameter to point to a 0 and the other one to point to something greater than 0
// note also that while num_ranks is the same everywhere, if you
// specify num_ranks, num_ranks_per_node may be different at different
// hpx ranks
int decide_ranks_per_node(int hpx_rank, int* num_ranks, int* num_ranks_per_node, int* min_rank_here) {
  int num_localities = hpx_get_num_ranks();
  int size;
  int num_ranks_here;

  if (*num_ranks_per_node != 0) {
    size = *num_ranks_per_node * num_localities;
    num_ranks_here = *num_ranks_per_node;
    if (min_rank_here != NULL)
      *min_rank_here = *num_ranks_per_node * hpx_get_my_rank();
  }
  else if (*num_ranks != 0) {
    size = *num_ranks;
    num_ranks_here = *num_ranks / num_localities;
    if (hpx_rank < (*num_ranks % num_localities))
      num_ranks_here += 1;
    if (min_rank_here != NULL)
      *min_rank_here = (*num_ranks / num_localities + 1) * hpx_get_my_rank();
  }
  else {
    size = num_localities;
    num_ranks_here = 1;
    if (min_rank_here != NULL)
      *min_rank_here = 0;
  }

  *num_ranks = size;
  *num_ranks_per_node = num_ranks_here;
  return SUCCESS;
}

int get_ranks_per_node(int hpx_rank, int* num_ranks, int* num_ranks_per_node, int* min_rank_here) {
  return  decide_ranks_per_node(hpx_rank, num_ranks, num_ranks_per_node, min_rank_here);
}

static void shutdown_action(void* args) {
  log_lock_fini();
  log_p2p_fini();
  log_generic_fini();
}

// this is called remotely by mpi_system_init for each HPX rank
static int init_action(struct init_args *args) {
  int num_ranks = args->num_ranks;
  int num_ranks_per_node = args->num_ranks_per;

  int num_localities = hpx_get_num_ranks();
  int hpx_rank = hpx_get_my_rank();
  int size = num_ranks;
  int num_ranks_here = num_ranks_per_node;;
  int min_rank_here = -1;

  // 1. figure out how many ranks we need at each location - we need this to alloc shared_state
  decide_ranks_per_node(hpx_rank, &size, &num_ranks_here, &min_rank_here);

  // 2. alloc and set state information we already calculated
  dbg_printf("Allocating mpi simulator shared state\n");
  struct mpi_shared_state *s = malloc(sizeof(*s));
  if (s == NULL)
    return ERROR;
  s->size = size;
  s->num_ranks_here = num_ranks_here;
  s->rank_init_futs = calloc(size, sizeof(s->rank_init_futs[0]));
  if (s->rank_init_futs == NULL)
    return ERROR;
  for (int i = 0; i < size; i++)
    s->rank_init_futs[i] = hpx_lco_future_new(0);
  s->tids = calloc(num_ranks_here, sizeof(*(s->tids)));
  if (s->tids == NULL)
    return ERROR;
  s->rankls = calloc(num_ranks_here, sizeof(*(s->rankls)));
  if (s->rankls == NULL)
    return ERROR;
  s->ranks_created = 0;

  shared_state = s;

  // TODO should probably make sure we free any storage on failure....

  // 3. set up remaining data
  //  s->curr_mpi_rank_available = -1 * num_localities;

  int success = log_lock_init();
  if (success != 0)
    return -1;

  dbg_printf("Done initing and allocing shared data\n");
  return HPX_SUCCESS;
}

static int mark_rank_inited(void *args) {
  struct rank_init_fut_args *s = (struct rank_init_fut_args*)args;
  hpx_lco_set(shared_state->rank_init_futs[s->rank], 0, NULL, HPX_NULL, HPX_NULL);
  dbg_printf("On hpx rank %d setting future for rank %d\n", hpx_get_rank(), s->rank);
  return HPX_SUCCESS;
}

void mpi_system_register_actions() {
  HPX_REGISTER_ACTION(&action_init, init_action);
  HPX_REGISTER_ACTION(&action_shutdown, shutdown_action);
  HPX_REGISTER_ACTION(&action_mark_rank_inited, mark_rank_inited);
  HPX_REGISTER_ACTION(&action_send_remote, send_remote);
  HPX_REGISTER_ACTION(&action_scatterv_recv, scatterv_recv);
  HPX_REGISTER_ACTION(&action_gatherv_recv, gatherv_recv);
  HPX_REGISTER_ACTION(&action_bcast_recv, bcast_recv);
  HPX_REGISTER_ACTION(&action_gather_recv, gather_recv);
  HPX_REGISTER_ACTION(&alltoallv_recv, alltoallv_recv);
  HPX_REGISTER_ACTION(&action_alltoall_recv, alltoall_recv);
  HPX_REGISTER_ACTION(&action_reduce_recv, reduce_recv);
}

// only needs to be called once from HPX root node
int mpi_system_init(int num_ranks, int ranks_per_node) {
  struct init_args args = {
    .num_ranks = num_ranks,
    .num_ranks_per = ranks_per_node
  };

  int num_hpx_ranks = hpx_get_num_ranks();
  hpx_addr_t and = hpx_lco_and_new(num_hpx_ranks);
  for (int i = 0; i < num_hpx_ranks; ++i)
    hpx_call(HPX_THERE(i), action_init, &args, sizeof(args), and);

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  dbg_printf("MPI simulator system setup\n");
  return SUCCESS;
}

int mpi_system_shutdown() {
 int num_hpx_ranks = hpx_get_num_ranks();
  hpx_addr_t and = hpx_lco_and_new(num_hpx_ranks);
  for (int i = 0; i < num_hpx_ranks; ++i)
    hpx_call(HPX_THERE(i), action_shutdown, NULL, 0, and);

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  return SUCCESS;
}

int mpi_init(int *dummy_argc, char*** dummy_argv) {
  int err;
  mpi_init_(&err);
  return err;
}

void mpi_init_(int* pier) {

  // reserve mpi rank
  //  int rank = sync_fadd(shared_state->curr_mpi_rank_available, shared_state->size, SYNC_SEQ_CST); // rank = -rank_per_node + i * totals_ranks
  int n = sync_fadd(&shared_state->ranks_created, 1, SYNC_SEQ_CST);

  // write entry in tids list
  shared_state->tids[n] = hpx_thread_get_tls_id();

  //hpx_thread_set_affinity(n % hpx_get_num_threads());

  // HERE

  // setup rank-specific and construct necessary dictionaries

  // create MPI_COMM_WORLD
  struct communicator *mpi_comm_world = malloc(sizeof(*mpi_comm_world));
  communicator_init(mpi_comm_world, shared_state->size, hpx_get_my_rank() + n * hpx_get_num_ranks());

  // set up rankls
  struct mpi_rank_rankls *rankls = &shared_state->rankls[n];
  rankls->initialized = true;
  rankls->finalized = 0;
  rankls->rank = hpx_get_my_rank() + n * hpx_get_num_ranks();
  rankls->num_comms = 1;
  rankls->comms = malloc(sizeof(shared_state->rankls[n].comms[0]));
  rankls->comms[0] = mpi_comm_world;
  rankls->requests_backing = calloc(sizeof(rankls->requests_backing[0]), REQUEST_CAPACITY);
  if (rankls->requests_backing == NULL) {
    *pier =  ERROR;
    return;
  }
  rankls->requests_backing[0].active = false; // set up the NULL_REQUEST
  rankls->requests_capacity = REQUEST_CAPACITY;


  // broadcast to all localities to set future for this thread
  for (int i = 0; i < hpx_get_num_ranks(); i++) {
    struct rank_init_fut_args args;
    args.rank = shared_state->rankls[n].rank;
    hpx_parcel_t *p = hpx_parcel_acquire(&args, sizeof(args));
    if (p == NULL) {
      // do some error handling
        hpx_abort();
    }

    hpx_parcel_set_action(p, action_mark_rank_inited);
    //    hpx_parcel_set_data(p, (void*)&args, sizeof(args));
    dbg_printf("On hpx rank %d, sending message to hpx rank %d to set future for mpi rank %d from thread %d\n", hpx_get_my_rank(), get_hpx_rank_from_mpi_rank(i), args.rank, n);
    hpx_parcel_set_target(p, HPX_THERE(get_hpx_rank_from_mpi_rank(i)));
    hpx_parcel_send(p, HPX_NULL);
  }

  for (int i = 0; i < shared_state->size; i++) {
    hpx_lco_get(shared_state->rank_init_futs[i], 0, NULL);
  }

  rankls->initialized = true;

  int success = log_p2p_init();
  success = log_generic_init();

  *pier = MPI_SUCCESS_;
}

//void mpi_comm_rank_(MPI_Comm comm, int* rank, int *pier) {
void mpi_comm_rank(int comm, int* rank) {
  int index = 0;
  int error = get_rankls_index(shared_state, hpx_thread_get_tls_id(), &index);

  *rank = shared_state->rankls[index].rank;
}

void mpi_comm_rank_(int *comm, int* rank, int *pier) {
  *pier = MPI_SUCCESS_;
  int index = 0;
  int error = get_rankls_index(shared_state, hpx_thread_get_tls_id(), &index);
  if (error != SUCCESS)
    *pier = ERROR;

  *rank = shared_state->rankls[index].rank;
}


int mpi_finalize() {
  int err;
  mpi_finalize_(&err);
  return 0;
}

void mpi_finalize_(int *pier) {
  // hack... TODO: replace with real barrier or better, an internal barrier
  int err;
  int send_buffer[2] = {0, 1};
  int count = 2;
  int root = 0;
  MPI_Datatype dt = MPI_INT;
  int comm = MPI_COMM_WORLD_;
  mpi_bcast_(&send_buffer, &count, &dt, &root, &comm, &err);
   //   int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype,
   //        int root, MPI_Comm comm)
  // end barrier

  struct mpi_rank_rankls *rankls = get_rankls(shared_state);

  sync_store(&rankls->finalized, 1, SYNC_RELAXED);
  int index = 0;
  get_rankls_index(shared_state, hpx_thread_get_tls_id(), &index);

  // TODO free all operation specific operations

  for (int i = 0; i < shared_state->rankls[index].num_comms; i++) {
    communicator_destroy(shared_state->rankls[index].comms[i]);
    free(shared_state->rankls[index].comms[i]);
  }

  *pier = MPI_SUCCESS_;
}

void mpi_comm_size_(int *comm, int* size, int *pier) {
  *pier = MPI_SUCCESS_;
  *size = shared_state->size; // todo fixme set specific to the communicator
}

void mpi_comm_size(int *comm, int* size) {
  *size = shared_state->size; // todo fixme set specific to the communicator
}

void mpi_type_size_(MPI_Datatype datatype, int *size, int *pier) {
  *pier = MPI_SUCCESS_;
  if (datatype >= 0 && datatype <= 2)
    *size = 1;
  else if (datatype >= 3 && datatype <= 5)
    *size = 2;
  else if (datatype >= 6 && datatype <= 9)
    *size = 4;
  else if (datatype == 10) //float
    *size = 4;
  else if (datatype == 11) // double
    *size = 8;
  else if (datatype == 12) // long double
    *size = 12;
  else if (datatype == 13) // long long int
    *size = 8;
  else if (datatype == 27) // double precision
    *size = 8;
  else if (datatype == 28) // int
    *size = 4;
  else if (datatype == 25) // logical
    *size = 2;
  else if (datatype == 33) // two double precision numbers
    *size = 16;
  else {
    *size = -1;
    *pier = ERROR;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////
// communicator //////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

struct p2p_match_args {
  struct communicator *comm;
};

void p2p_match_main(void* vargs);

int communicator_init(struct communicator *comm, int size, int rank) {
  comm->rank = rank;
  comm->size = size;
  comm->op_dict = malloc(sizeof(*comm->op_dict));
  communicator_op_dict_init(comm->op_dict);
  // create and allocate transaction dicts for each operation type

  comm->requests_lock = hpx_lco_sema_new(1);

  list_init(&comm->requests);

  comm->p2p_transaction_lists = malloc(sizeof(comm->p2p_transaction_lists[0]) * comm->size);
  if (comm->p2p_transaction_lists == NULL)
    return ERROR;
  for (int i = 0; i < comm->size; i++) {
    struct communicator_p2p_transaction_list *list = malloc(sizeof(*list));
    communicator_p2p_transaction_list_init(list, comm, rank, i);
    comm->p2p_transaction_lists[i] = list;
  }

  struct communicator_transaction_dict *scatterv_dict = malloc(sizeof(*scatterv_dict));
  communicator_transaction_dict_init(scatterv_dict);
  communicator_op_dict_insert(comm->op_dict, OP_SCATTERV, scatterv_dict);

  struct communicator_transaction_dict *gatherv_dict = malloc(sizeof(*gatherv_dict));
  communicator_transaction_dict_init(gatherv_dict);
  communicator_op_dict_insert(comm->op_dict, OP_GATHERV, gatherv_dict);

  struct communicator_transaction_dict *bcast_dict = malloc(sizeof(*bcast_dict));
  communicator_transaction_dict_init(bcast_dict);
  communicator_op_dict_insert(comm->op_dict, OP_BCAST, bcast_dict);

  struct communicator_transaction_dict *gather_dict = malloc(sizeof(*gather_dict));
  communicator_transaction_dict_init(gather_dict);
  communicator_op_dict_insert(comm->op_dict, OP_GATHER, gather_dict);

  struct communicator_transaction_dict *alltoallv_dict = malloc(sizeof(*alltoallv_dict));
  communicator_transaction_dict_init(alltoallv_dict);
  communicator_op_dict_insert(comm->op_dict, OP_ALLTOALLV, alltoallv_dict);

  struct communicator_transaction_dict *alltoall_dict = malloc(sizeof(*alltoall_dict));
  communicator_transaction_dict_init(alltoall_dict);
  communicator_op_dict_insert(comm->op_dict, OP_ALLTOALL, alltoall_dict);

  struct communicator_transaction_dict *reduce_dict = malloc(sizeof(*reduce_dict));
  communicator_transaction_dict_init(reduce_dict);
  communicator_op_dict_insert(comm->op_dict, OP_REDUCE, reduce_dict);

  struct p2p_match_args *args = malloc(sizeof(*args));
  args->comm = comm;

#if USE_P2P_MATCH_THREAD
  hpx_thread_create(__hpx_global_ctx, 0, (hpx_action_handler_t)p2p_match_main, args, &comm->p2p_match_fut, NULL);
#endif

  return SUCCESS;
}

int communicator_destroy(struct communicator *comm) {
#if USE_P2P_MATCH_THREAD
  dbg_printf("About to wait on p2p_match_fut\n");
  hpx_thread_wait(comm->p2p_match_fut);
#endif
  // todo
  // destroy requests list
  // destroy p2p_transaction_lists
  // destroy op_dict
  // free p2p transaction_dicts
  // hpx_free(comm->op_dict);
  return SUCCESS;
}

static int communicator_lock(struct communicator *comm) {
  struct lock_record* record = log_lock_start(COMM_P, comm->rank, comm->rank);
  hpx_lco_sema_p(comm->requests_lock);
  log_lock_end(record);
  return SUCCESS;
}

static int communicator_unlock(struct communicator *comm) {
  struct lock_record* record = log_lock_start(COMM_V, comm->rank, comm->rank);
  hpx_lco_sema_v(comm->requests_lock);
  log_lock_end(record);
  return SUCCESS;
}


int communicator_push_active_request(struct communicator* comm, struct mpi_request_ *req) {
  communicator_lock(comm);
  req->comm = comm;
  list_push_back(&comm->requests, req);
  communicator_unlock(comm);
  return SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////
// communicator dict /////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

int communicator_op_dict_init(struct communicator_op_dict *d) {
  d->lock = hpx_lco_sema_new(1);
  d->size = 0;
  d->capacity = DICT_CAPACITY;
  d->ops = malloc(DICT_CAPACITY * sizeof(*d->ops));
  if (d->ops == NULL)
    return ERROR;

  d->transaction_dicts = malloc(DICT_CAPACITY * sizeof(*d->transaction_dicts[0]));
  if (d->transaction_dicts == NULL)
    return ERROR;

  return SUCCESS;
}

//int communicator_op_dict_insert(struct communicator_op_dict *d, MPI_Comm comm, struct communicator_transaction_dict *td) {
int communicator_op_dict_insert(struct communicator_op_dict *d, int op, struct communicator_transaction_dict *td) {
  if (d->size >= d->capacity)
    return ERROR;

  hpx_lco_sema_p(d->lock);
  d->ops[d->size] = op;
  d->transaction_dicts[d->size] = td;
  ++d->size;
  hpx_lco_sema_v(d->lock);
  return SUCCESS;
}

//int communicator_op_dict_search(struct communicator_op_dict *d, MPI_Comm comm, struct communicator_transaction_dict **td) {
int communicator_op_dict_search(struct communicator_op_dict *d, int op, struct communicator_transaction_dict **td) {
  hpx_lco_sema_p(d->lock);
  int i = 0;
  while (d->ops[i] != op && i < d->size)
    ++i;
  if (i == d->size) {
    *td = NULL;
    hpx_lco_sema_v(d->lock);
    return ERROR;
  }
  else
    *td = d->transaction_dicts[i];
  hpx_lco_sema_v(d->lock);
  return SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////
// communicator transaction dict /////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

int communicator_transaction_dict_init(struct communicator_transaction_dict *d) {
  d->transaction_count_nicside = 0;
  d->transaction_count_userside = 0;
  d->lock = hpx_lco_sema_new(1);
  d->size      = 0;
  d->capacity  = DICT_CAPACITY;
  d->trans_num  = malloc(d->capacity * sizeof(d->trans_num));
  if (d->trans_num == NULL)
    return ERROR;
  d->nicside_fut = malloc(d->capacity * sizeof(d->nicside_fut));
  if (d->nicside_fut == NULL)
    return ERROR;
  d->userside_fut = malloc(d->capacity * sizeof(d->userside_fut));
  if (d->userside_fut == NULL)
    return ERROR;
  d->completed = malloc(d->capacity * sizeof(*d->completed));
  if (d->completed == NULL)
    return ERROR;
  for (int i = 0; i < d->capacity; ++i)
    d->completed[i] = true;
  return SUCCESS;
}


/* This should only be called by a function that has locked the table already */
static int communicator_transaction_dict_resize(struct communicator_transaction_dict *d) {
  d->capacity     = d->capacity * 2;
  d->trans_num    = realloc(d->trans_num, d->capacity * sizeof(d->trans_num));
  d->nicside_fut  = realloc(d->nicside_fut, d->capacity * sizeof(d->nicside_fut));
  d->userside_fut = realloc(d->userside_fut, d->capacity * sizeof(d->userside_fut));
  d->completed = realloc(d->completed, d->capacity * sizeof(*(d->completed)));

  if (d->trans_num == NULL || d->nicside_fut == NULL || d->userside_fut == NULL || d->completed == NULL)
    return ERROR;

  return SUCCESS;
}

int communicator_transaction_dict_complete(struct communicator_transaction_dict *d, int transaction_num) {
  hpx_lco_sema_p(d->lock);
  int i = 0;

  while (d->trans_num[i] != transaction_num && i < d->capacity)
    ++i;

  if (i >= d->capacity) {
    hpx_lco_sema_v(d->lock);
    return ERROR;
  }

  d->completed[i] = true;
  d->size--;

  hpx_lco_sema_v(d->lock);

  return SUCCESS;
}

// should only be called by function that has the lock
static int communicator_transaction_dict_add_entry(struct communicator_transaction_dict *d, int *index) {
    int i= 0;
    while (d->completed[i] != true  && i < d->capacity)
      ++i;
    if (i == d->capacity) {
      int error = communicator_transaction_dict_resize(d);
      if (error != SUCCESS)
    return ERROR;
    }
    d->nicside_fut[i] = hpx_lco_future_new(0);
    d->userside_fut[i] = hpx_lco_future_new(0);
    d->completed[i] = false;
    d->size++;

    *index = i;
    return SUCCESS;
}

int communicator_transaction_dict_userside_record(struct communicator_transaction_dict *d, int transaction_num, void* val_p, hpx_addr_t* fut) {
  hpx_lco_sema_p(d->lock);
  int i = 0;

  // is the transaction already here?
  for (i = 0; i < d->capacity; i++) {
    if (d->trans_num[i] == transaction_num && d->completed[i] == false)
      break;
  }
  //  while (i < d->capacity && d->trans_num[i] != transaction_num && d->completed[i] == true) // BAD
  //    ++i;

  if (i >= d->capacity) { // i.e. is not already in the table
    int error = communicator_transaction_dict_add_entry(d, &i);
    if (error != SUCCESS) {
      hpx_lco_sema_v(d->lock);
      return error;
    }
    d->trans_num[i] = transaction_num;
  }

  if (fut != NULL)
    *fut = d->userside_fut[i];
  hpx_lco_set(d->nicside_fut[i], sizeof(val_p), &val_p, HPX_NULL, HPX_NULL);

  hpx_lco_sema_v(d->lock);

  return SUCCESS;
}

// if necessary this will insert a new entry in the transaction table
// if the transaction is already in the table, this will merely set the data
int communicator_transaction_dict_nicside_record(struct communicator_transaction_dict *d, int transaction_num, hpx_addr_t* userside_fut, hpx_addr_t* nicside_fut) {
  hpx_lco_sema_p(d->lock);
  int i = 0;

  // is the transaction already here?
  for (i = 0; i < d->capacity; i++) {
    if (d->trans_num[i] == transaction_num && d->completed[i] == false)
      break;
  }
  //  while (d->trans_num[i] != transaction_num && i < d->capacity && d->completed[i] != true)
  //    ++i;

  if (i >= d->capacity) { // i.e. is not already in the table
    int error = communicator_transaction_dict_add_entry(d, &i);
    if (error != SUCCESS) {
      hpx_lco_sema_v(d->lock);
      return error;
    }
    d->trans_num[i] = transaction_num;
  }

  if (userside_fut != NULL)
    *userside_fut = d->userside_fut[i];
  if (nicside_fut != NULL)
    *nicside_fut = d->nicside_fut[i];

  hpx_lco_sema_v(d->lock);

  return SUCCESS;
}

int communicator_transaction_dict_nicside_future(struct communicator_transaction_dict *d, int transaction_num, hpx_addr_t* fut) {
  hpx_lco_sema_p(d->lock);
  int i = 0;

  for (i = 0; i < d->capacity; i++) {
    if (d->trans_num[i] == transaction_num && d->completed[i] == false)
      break;
  }

  if (fut != NULL)
    *fut = d->nicside_fut[i];

  hpx_lco_sema_v(d->lock);

  return SUCCESS;
}


int communicator_transaction_dict_userside_future(struct communicator_transaction_dict *d, int transaction_num, hpx_addr_t* fut) {
  hpx_lco_sema_p(d->lock);
  int i = 0;

  for (i = 0; i < d->capacity; i++) {
    if (d->trans_num[i] == transaction_num && d->completed[i] == false)
      break;
  }

  if (fut != NULL)
    *fut = d->userside_fut[i];

  hpx_lco_sema_v(d->lock);

  return SUCCESS;
}

// transaction_num [out]  - the new transaction number
int communicator_transaction_dict_inc_nicside_count(struct communicator_transaction_dict *d, int* transaction_num) {
  *transaction_num = sync_fadd(&d->transaction_count_nicside, 1, SYNC_SEQ_CST);
  return SUCCESS;
}

// transaction_num [out]  - the new transaction number
int communicator_transaction_dict_inc_userside_count(struct communicator_transaction_dict *d, int* transaction_num) {
  *transaction_num = sync_fadd(&d->transaction_count_userside, 1, SYNC_SEQ_CST);
  return SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////
// point to point transaction dict ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

static int communicator_p2p_transaction_list_fulfill(struct communicator_p2p_transaction_list *d, struct mpi_request_* req, struct p2p_msg* msg);

int communicator_p2p_transaction_list_init(struct communicator_p2p_transaction_list *d, struct communicator* comm, int src, int dest) {
  d->comm = comm;
  d->src = src;
  d->dest = dest;
  d->lock = hpx_lco_sema_new(1);
  d->transaction_count_send = 0;
  d->curr_trans_num = -1;
  list_init(&d->received_messages);
  return SUCCESS;
}

// transaction_num [out]  - the new transaction number
int communicator_p2p_transaction_list_inc_send_count(struct communicator_p2p_transaction_list *d, int* transaction_num) {
  *transaction_num = sync_fadd(&d->transaction_count_send, 1, SYNC_SEQ_CST);
  return SUCCESS;
}

static inline int communicator_p2p_transaction_list_lock(struct communicator_p2p_transaction_list *d) {
  struct lock_record* record = log_lock_start(P2P_P, d->src, d->dest);
  hpx_lco_sema_p(d->lock);
  log_lock_end(record);
  return SUCCESS;
}

static inline int communicator_p2p_transaction_list_unlock(struct communicator_p2p_transaction_list *d) {
  struct lock_record* record = log_lock_start(P2P_V, d->src, d->dest);
  hpx_lco_sema_v(d->lock);
  log_lock_end(record);
  return SUCCESS;
}

int communicator_p2p_transaction_list_append(struct communicator_p2p_transaction_list *d, struct p2p_msg* data) {
  communicator_p2p_transaction_list_lock(d);

  data->nicside_fut = hpx_lco_future_new(0);

  if (list_size(&d->received_messages) == 0) {
    list_push_front(&d->received_messages, data);
    if (data->trans_num == d->curr_trans_num + 1)
      d->curr_trans_num++;
  }
  else {
    list_node_t *node = list_first(&d->received_messages);
    struct p2p_msg* list_msg = (struct p2p_msg*)node->value;
    if (list_msg->trans_num > data->trans_num) {
      list_push_front(&d->received_messages, data);
      // if this is the front item on the list, we may need to update the transaction lists's curr transaction number
      if (data->trans_num == d->curr_trans_num + 1)
    d->curr_trans_num++;
    }
    else {
      // we need to insert this somewhere on the list
      list_node_t *node_after = node;
      node = node->next;
      while (node != NULL) {
    list_msg = (struct p2p_msg*)node->value;
    if (list_msg->trans_num < data->trans_num)
      node_after = node;
    node = node->next;
      }
      list_insert_after(&d->received_messages, node_after->value, data);
    }
  }
  communicator_p2p_transaction_list_unlock(d);
  return SUCCESS;
}

int communicator_p2p_transaction_list_try_fulfill(struct communicator_p2p_transaction_list *d, struct mpi_request_* req, bool *success) {
  if (d->curr_trans_num < 0) { // can't fulfill a request if we have no data
    return SUCCESS;
  }

  communicator_p2p_transaction_list_lock(d);
  // see if there is a relevant future
  list_node_t *node = list_first(&d->received_messages);
  int last_seen_trans_num = d->curr_trans_num;
  struct p2p_msg* msg = NULL;
  bool matched = false;
  while (node != NULL) {
    msg = (struct p2p_msg*)node->value;
    // if trans_num skips more than 1, we could break MPI semantics by matching a message past that :
    if (msg->trans_num >= last_seen_trans_num + 1) {
      matched = false;
      break;
    }
    if (req->tag == MPI_ANY_TAG_ || req->tag == msg->tag) {
      matched = true;
      break;
    }
    last_seen_trans_num++;
    node = list_next(node);
  }

  *success = matched;
  int retval = SUCCESS;
  if (matched)
    retval = communicator_p2p_transaction_list_fulfill(d, req, msg);
  communicator_p2p_transaction_list_unlock(d);
  return retval;
}

// handles matching the message with its request, removes the message from the list, and changes the current transaction number
// should only be called by function that has the lock
static int communicator_p2p_transaction_list_fulfill(struct communicator_p2p_transaction_list *d, struct mpi_request_* req, struct p2p_msg* msg) {
  hpx_lco_set(msg->nicside_fut, sizeof(req), &req, HPX_NULL, HPX_NULL);
  list_delete(&d->received_messages, msg);
  d->curr_trans_num++;
  return SUCCESS;
}

int match_message(struct communicator* comm, bool *success) {
  struct generic_record* record = log_generic_start(G_DURATION);
  communicator_lock(comm);
  bool matched = false;
  list_node_t *node = list_first(&comm->requests);
  struct mpi_request_ *req = NULL;
  if (node != NULL) {
    req = (struct mpi_request_*)node->value;

    if (req->src != MPI_ANY_SOURCE_)
      communicator_p2p_transaction_list_try_fulfill(comm->p2p_transaction_lists[req->src], req, &matched);
    else {
      for (int i = 0; i < comm->size; i++) {
    communicator_p2p_transaction_list_try_fulfill(comm->p2p_transaction_lists[i], req, &matched);
    if (matched)
      break;
      }
    }
  }
  if (matched) {
    *success = matched;
    //    req->active = false;
    list_pop(&comm->requests);
  }
  communicator_unlock(comm);

  log_generic_end(record);

  return SUCCESS;
}

void p2p_match_main(void* vargs) {
  struct p2p_match_args* args = vargs;
  struct mpi_rank_rankls *rankls = get_rankls(shared_state);
  int finalized;
  sync_load(finalized, &rankls->finalized, SYNC_RELAXED);
  while (!finalized) {
    bool matched;
    for (int i = 0; i < 100; i++)
      match_message(args->comm, &matched);
    //    hpx_thread_yield();
    sync_load(finalized, &rankls->finalized, SYNC_RELAXED);
  }
}
