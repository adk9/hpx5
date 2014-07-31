
#include "main.h"

int _comm_parent_result_action(ParentNodalArgs *args)
{
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;

  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  // 0. wait for the right generation to become active
  hpx_lco_gencount_wait(ld->epoch, args->epoch);

  int size = args->size;
  int srcIndex = args->srcIndex;
  int i = args->i;
  int iter = args->iter;
  int *incoming_buf = args->buf;

  // 1. acquire the domain lock
  hpx_lco_sema_p(ld->sem_parent);

  // 2. update
  int j,b;
  parent *pp;
  for (j = 0; j < ld->par_p.comm_num[i]; j++) {
    if (incoming_buf[ld->par_p.index[i]+j] > -1) {
      pp = &ld->parents[ld->par_p.comm_p[ld->par_p.index[i]+j]]; 
      pp->refine = 0;
      for (b=0;b<8;b++) {
        if (pp->child_node[b] == ld->my_pe && pp->child[b] >= 0 &&
            ld->blocks[pp->child[b]].refine == -1) {
          ld->blocks[pp->child[b]].refine = 0;
        }
      } 
    }
  }

  // 3. release the domain lock
  hpx_lco_sema_v(ld->sem_parent);

  // 4. join the and for this epoch---the _advanceDomain action is waiting on
  //    this before it performs local computation for the epoch
  hpx_lco_and_set(ld->parent_and[args->epoch % 2 + 2*iter], HPX_NULL);

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

int _comm_parent_sends_action(parentSBN *psbn)
{
  hpx_addr_t local = hpx_thread_current_target();

  Domain *ld = psbn->domain;
  int i = psbn->i;
  int iter = psbn->iter;

  // Acquire a large-enough buffer to pack into.
  // - NULL first parameter means it comes with the parcel and is managed by
  //   the parcel and freed by the system inside of send()
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(ParentNodalArgs) + ld->par_b.comm_num[i]*sizeof(int));
  assert(p);

  // "interpret the parcel buffer as a Nodal"
  ParentNodalArgs *nodal = hpx_parcel_get_data(p);

  nodal->size = ld->par_b.comm_num[i];
  nodal->epoch = psbn->epoch;
  nodal->srcIndex = psbn->rank;
  nodal->i = i;
  nodal->iter = iter;

  int n;
  for (n=0;n<ld->par_b.comm_num[i];n++) {
    if ( ld->par_b.comm_b[ld->par_b.index[i]+n] < 0 ) {
      nodal->buf[n] = 0;
    } else {
      nodal->buf[n] = ld->blocks[ld->par_b.comm_b[ld->par_b.index[i]+n]].refine;
    }
  }

  int dest = ld->par_b.comm_part[i];
  int distance = -psbn->rank + dest;
  hpx_addr_t neighbor = hpx_addr_add(local, sizeof(Domain) * distance);

  hpx_parcel_set_target(p, neighbor);
  hpx_parcel_set_action(p, _comm_parent_result);
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

// These routines are concerned with communicating information between
// parents and their children.  This includes information about refinement
// level and also includes a routine to keep track to where children are
// being moved to.  For on node children, the parent has the index of the
// child block and for off node children has the block number.
void comm_parent(Domain *ld,unsigned long epoch,int iter)
{
   int i;
   hpx_addr_t local = hpx_thread_current_target();

   // you may have to re-allocate the next generation if the grid changes
   ld->parent_and[(epoch + 1) % 2 + 2*iter] = hpx_lco_and_new(ld->par_p.num_comm_part);

   hpx_addr_t sends = hpx_lco_and_new(ld->par_b.num_comm_part);

   for (i = 0; i < ld->par_b.num_comm_part; i++) {
     hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(parentSBN));
     assert(p);

     parentSBN *psbn = hpx_parcel_get_data(p);

     psbn->rank             = ld->my_pe;
     psbn->i                = i;
     psbn->domain           = ld;
     psbn->epoch            = epoch;
     psbn->iter             = iter;

     hpx_parcel_set_target(p, local);
     hpx_parcel_set_action(p, _comm_parent_sends);
     hpx_parcel_set_cont_target(p, sends);
     hpx_parcel_set_cont_action(p, hpx_lco_set_action);

     // async is fine, since we're waiting on sends below
     hpx_parcel_send(p, HPX_NULL);
   }
   hpx_lco_wait(sends);
   hpx_lco_delete(sends, HPX_NULL);

   hpx_lco_gencount_inc(ld->epoch, HPX_NULL);
   hpx_lco_wait(ld->parent_and[epoch % 2 + 2*iter]);
   hpx_lco_delete(ld->parent_and[epoch % 2 + 2*iter], HPX_NULL);
}
