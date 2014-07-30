
#include "main.h"

int _comm_reverse_refine_result_action(RefineNodalArgs *args)
{
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;

  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;
  
  // 0. wait for the right generation to become active
  hpx_lco_gencount_wait(ld->epoch, args->epoch);

  int size = args->size;
  int srcIndex = args->srcIndex;
  int *incoming_buf = args->buf;
  int i = args->i;
  int dir = args->dir;
  int iter = args->iter;

  // 1. acquire the domain lock
  hpx_lco_sema_p(ld->sem_reverse_refine);

  // 2. update
  block *bp;
  int n,c;
  for (n = 0; n < ld->comm_num[dir][i]; n++) {
    if (incoming_buf[ld->comm_index[dir][i]+n] >
        ld->blocks[ld->comm_block[dir][ld->comm_index[dir][i]+n]].refine) {
      bp = &ld->blocks[ld->comm_block[dir][ld->comm_index[dir][i]+n]];
      bp->refine = incoming_buf[ld->comm_index[dir][i]+n];
      if (bp->parent != -1 && bp->parent_node == ld->my_pe)
         if (ld->parents[bp->parent].refine == -1) {
           ld->parents[bp->parent].refine = 0;
           for (c = 0; c < 8; c++)
             if (ld->parents[bp->parent].child_node[c] == ld->my_pe &&
                 ld->parents[bp->parent].child[c] >= 0 &&
                 ld->blocks[ld->parents[bp->parent].child[c]].refine == -1)
               ld->blocks[ld->parents[bp->parent].child[c]].refine = 0;
         }
    }
  }

  // 3. release the domain lock
  hpx_lco_sema_v(ld->sem_reverse_refine);

  // 4. join the and for this epoch---the _advanceDomain action is waiting on
  //    this before it performs local computation for the epoch
  hpx_lco_and_set(ld->reverse_refine_and[args->epoch % 2 + 2*iter], HPX_NULL);

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

int _comm_reverse_refine_sends_action(refineSBN *psbn)
{
  hpx_addr_t local = hpx_thread_current_target();

  Domain *ld = psbn->domain;
  int dir = psbn->dir;
  int i = psbn->i;
  int iter = psbn->iter;

  // Acquire a large-enough buffer to pack into.
  // - NULL first parameter means it comes with the parcel and is managed by
  //   the parcel and freed by the system inside of send()
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(RefineNodalArgs) + ld->comm_num[dir][i]*sizeof(int));
  assert(p);

  // "interpret the parcel buffer as a Nodal"
  RefineNodalArgs *nodal = hpx_parcel_get_data(p);

  nodal->size = ld->comm_num[dir][i];
  nodal->epoch = psbn->epoch;
  nodal->srcIndex = psbn->rank;
  nodal->i = i;
  nodal->dir = dir;
  nodal->iter = iter;

  int n,face;
  for (n=0;n<ld->comm_num[dir][i];n++) {
    face = dir*2 + (ld->comm_face_case[dir][ld->comm_index[dir][i]+n] >= 10);
    nodal->buf[n] = ld->blocks[ld->comm_block[dir][ld->comm_index[dir][i]+n]].nei_refine[face];
  }

  int dest = ld->comm_partner[dir][i];
  int distance = -psbn->rank + dest;
  hpx_addr_t neighbor = hpx_addr_add(local, sizeof(Domain) * distance);

  hpx_parcel_set_target(p, neighbor);
  hpx_parcel_set_action(p, _comm_reverse_refine_result);
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;

}

void comm_reverse_refine(Domain *ld,unsigned long epoch,int iter)
{
   hpx_addr_t local = hpx_thread_current_target();

   // find out how many sends
   int nsends = 0;
   int dir,i;
   for (dir = 0; dir < 3; dir++) {
     nsends += ld->num_comm_partners[dir];
   }

   // you may have to re-allocate the next generation if the grid changes
   ld->reverse_refine_and[(epoch + 1) % 2 + 2*iter] = hpx_lco_and_new(nsends);

   hpx_addr_t sends = hpx_lco_and_new(nsends);

   for (dir = 0; dir < 3; dir++) {
      for (i = 0; i < ld->num_comm_partners[dir]; i++) {
         hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(refineSBN));
         assert(p);

         refineSBN *psbn = hpx_parcel_get_data(p);

         psbn->rank             = ld->my_pe;
         psbn->dir              = dir;
         psbn->i                = i;
         psbn->domain           = ld;
         psbn->epoch            = epoch;
         psbn->iter             = iter;

         hpx_parcel_set_target(p, local);
         hpx_parcel_set_action(p, _comm_reverse_refine_sends);
         hpx_parcel_set_cont_target(p, sends);
         hpx_parcel_set_cont_action(p, hpx_lco_set_action);

         // async is fine, since we're waiting on sends below
         hpx_parcel_send(p, HPX_NULL);
      }
   }

   hpx_lco_wait(sends);
   hpx_lco_delete(sends, HPX_NULL);

   hpx_lco_gencount_inc(ld->epoch, HPX_NULL);
   hpx_lco_wait(ld->reverse_refine_and[epoch % 2 + 2*iter]);
   hpx_lco_delete(ld->reverse_refine_and[epoch % 2 + 2*iter], HPX_NULL);
}
