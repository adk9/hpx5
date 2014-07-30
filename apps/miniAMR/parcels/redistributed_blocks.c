
#include "main.h"

// Redistribute blocks so that the number of blocks will not exceed the
// number of available blocks on processors during refinement and coarsening
void redistribute_blocks(double *tp, double *tm, double *tu, double *time,
                         int *num_moved, int num_split,Domain *ld, unsigned long epoch,int *iter)
{
   int i, in, m, n, p, need, excess, my_need, my_excess, target, rem, sum,
       my_active, space[ld->num_pes], use[ld->num_pes];
   block *bp;
   parent *pp;

   hpx_time_t t1 = hpx_time_now();

   for (i = 0; i < ld->num_pes; i++)
      ld->bin[i] = 0;
   ld->bin[ld->my_pe] = num_split;

   hpx_lco_set(ld->rcb_sumint,(ld->num_pes)*sizeof(int),ld->bin,HPX_NULL,HPX_NULL);
   hpx_lco_set(ld->rcb_sumint,(ld->num_pes)*sizeof(int),ld->gbin,HPX_NULL,HPX_NULL);

   for (sum = i = 0; i < ld->num_pes; i++) {
      ld->from[i] = 0;
      sum += ld->gbin[i];
   }

   for (i = 0; i < ld->num_pes; i++)
      ld->bin[i] = 0;
   ld->bin[ld->my_pe] = ld->max_num_parents - ld->num_parents - 1 - num_split;

   hpx_lco_set(ld->rcb_sumint,(ld->num_pes)*sizeof(int),ld->bin,HPX_NULL,HPX_NULL);

   for (in = 0; in < ld->sorted_index[ld->num_refine+1]; in++)
      ld->blocks[ld->sorted_list[in].n].new_proc = -1;

   target = sum/ld->num_pes;
   rem = sum - target*ld->num_pes;

   hpx_lco_set(ld->rcb_sumint,(ld->num_pes)*sizeof(int),space,HPX_NULL,HPX_NULL);

   for (excess = i = 0; i < ld->num_pes; i++) {
      need = target + (i < rem);
      if (need > space[i]) {
         use[i] = space[i];
         excess += need - space[i];
      } else
         use[i] = need;
   }
   // loop while there is blocks to be moved and progress is being made
   // if there are blocks to move and no progress, the code will fail later
   while (excess && sum)
      for (sum = i = 0; i < ld->num_pes && excess; i++)
         if (space[i] > use[i]) {
            use[i]++;
            excess--;
            sum++;
         }

   m = in = 0;
   if (num_split > use[ld->my_pe]) {  // have blocks to give
      my_excess = num_split - use[ld->my_pe];
      my_active = ld->num_active - my_excess + 7*use[ld->my_pe] + 1;
      (*num_moved) += my_excess;
      for (excess = i = 0; i < ld->my_pe; i++)
         if (ld->gbin[i] > use[i])
            excess += ld->gbin[i] - use[i];
      for (need = i = 0; i < ld->num_pes && my_excess; i++)
         if (ld->gbin[i] < use[i]) {
            need += use[i] - ld->gbin[i];
            if (need > excess)
               for ( ; in < ld->sorted_index[ld->num_refine+1] && need > excess &&
                       my_excess; in++)
                  if ((bp = &ld->blocks[ld->sorted_list[in].n])->refine == 1) {
                     ld->from[i]++;
                     bp->new_proc = i;
                     need--;
                     my_excess--;
                     m++;
                  }
         }
   } else  // getting blocks
      my_active = ld->num_active + 7*use[ld->my_pe] + 1;

   for (in = 0; in < ld->sorted_index[ld->num_refine+1]; in++) {
      n = ld->sorted_list[in].n;
      if ((bp = &ld->blocks[n])->number >= 0)
         if (bp->refine == -1 && bp->parent_node != ld->my_pe) {
            bp->new_proc = bp->parent_node;
            ld->from[bp->parent_node]++;
            my_active--;
            m++;
            (*num_moved)++;
         }
   }
   for (p = 0; p < ld->max_active_parent; p++)
      if ((pp = &ld->parents[p])->number >= 0 && pp->refine == -1)
         for (i = 0; i < 8; i++)
            if (pp->child_node[i] != ld->my_pe)
               my_active++;
            else
               ld->blocks[pp->child[i]].new_proc = ld->my_pe;

   // FIXME
   //MPI_Allreduce(&m, &n, 1, MPI_INTEGER, MPI_SUM, MPI_COMM_WORLD);

   if (n) {
      // FIXME
      //MPI_Allreduce(&my_active, &sum, 1, MPI_INTEGER, MPI_MAX, MPI_COMM_WORLD);

      if (sum > ((int) (0.75*((double) ld->max_num_blocks)))) {
         // even up the expected number of blocks per processor
         for (i = 0; i < ld->num_pes; i++)
            ld->bin[i] = 0;
         ld->bin[ld->my_pe] = my_active;

         hpx_lco_set(ld->rcb_sumint,(ld->num_pes)*sizeof(int),ld->bin,HPX_NULL,HPX_NULL);
         hpx_lco_set(ld->rcb_sumint,(ld->num_pes)*sizeof(int),ld->gbin,HPX_NULL,HPX_NULL);

         for (sum = i = 0; i < ld->num_pes; i++)
            sum += ld->gbin[i];

         target = sum/ld->num_pes;
         rem = sum - target*ld->num_pes;

         in = ld->sorted_index[ld->num_refine+1] - 1;  // don't want to move big blocks
         if (my_active > (target + (ld->my_pe < rem))) {  // have blocks to give
            my_excess = my_active - (target + (ld->my_pe < rem));
            (*num_moved) += my_excess;
            for (excess = i = 0; i < ld->my_pe; i++)
               if (ld->gbin[i] > (target + (i < rem)))
                  excess += ld->gbin[i] - (target + (i < rem));
            for (need = i = 0; i < ld->num_pes && my_excess; i++)
               if (ld->gbin[i] < (target + (i < rem))) {
                  need += (target + (i < rem)) - ld->gbin[i];
                  if (need > excess)
                     for ( ; in >= 0 && need > excess && my_excess; in--)
                        if ((bp = &ld->blocks[ld->sorted_list[in].n])->new_proc == -1){
                           ld->from[i]++;
                           bp->new_proc = i;
                           need--;
                           my_excess--;
                           m++;
                        }
               }
         }
      } else
         in = ld->sorted_index[ld->num_refine+1] - 1;

      // keep the rest of the blocks on this processor
      for ( ; in >= 0; in--)
         if (ld->blocks[ld->sorted_list[in].n].new_proc == -1)
            ld->blocks[ld->sorted_list[in].n].new_proc = ld->my_pe;

      *time = hpx_time_elapsed_ms(t1);

      // FIXME
      //MPI_Alltoall(from, 1, MPI_INTEGER, to, 1, MPI_INTEGER, MPI_COMM_WORLD);
      //move_blocks(tp, tm, tu);
   } else
      *time = hpx_time_elapsed_ms(t1);
}

