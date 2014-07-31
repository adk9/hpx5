
#include "main.h"

// Reset the neighbor lists on blocks so that matching them against objects
// can set those which can be refined.
void reset_all(Domain *ld)
{
   int n, c, in;
   block *bp;
   parent *pp;

   for (in = 0; in < ld->sorted_index[ld->num_refine+1]; in++) {
      n = ld->sorted_list[in].n;
      if ((bp= &ld->blocks[n])->number >= 0) {
         bp->refine = -1;
         for (c = 0; c < 6; c++)
            if (bp->nei_level[c] >= 0)
               bp->nei_refine[c] = -1;
      }
   }

   for (n = 0; n < ld->max_active_parent; n++)
      if ((pp = &ld->parents[n])->number >= 0) {
         pp->refine = -1;
         for (c = 0; c < 8; c++)
            if (pp->child[c] < 0)
               pp->refine = 0;
         if (pp->refine == 0)
            for (c = 0; c < 8; c++)
               if (pp->child_node[c] == ld->my_pe && pp->child[c] >= 0)
                  if (ld->blocks[pp->child[c]].refine == -1)
                     ld->blocks[pp->child[c]].refine = 0;
      }
}

void refine(int ts,Domain *ld,unsigned long epoch)
{
  int i, j, n, p, c, in, min_b, max_b, sum_b, num_refine_step, num_split;
   double ratio, tp, tm, tu, tp1, tm1, tu1, t1, t3, t4;
   hpx_time_t t5;
   block *bp;
   parent *pp;

   ld->nrs++;
   t4 = tp = tm = tu = tp1 = tm1 = tu1 = 0.0;

   hpx_time_t t2 = hpx_time_now();

   hpx_lco_set(ld->rsum,(ld->num_refine+1)*sizeof(int),ld->local_num_blocks,HPX_NULL,HPX_NULL);

   hpx_lco_get(ld->rsum,(ld->num_refine+1)*sizeof(int),ld->num_blocks);


   ld->timer_refine_sy += hpx_time_elapsed_ms(t2);
   t4 += hpx_time_elapsed_ms(t2);

   if (ts) 
     num_refine_step = ld->block_change;
   else
     num_refine_step = ld->num_refine;

   int count = 0;
   for (i = 0; i < num_refine_step; i++) {
      for (j = ld->num_refine; j >= 0; j--)
         if (ld->num_blocks[j]) {
            ld->cur_max_level = j;
            break;
      }
      reset_all(ld);
      if (ld->uniform_refine) {
         for (in = 0; in < ld->sorted_index[ld->num_refine+1]; in++) {
            n = ld->sorted_list[in].n;
            bp = &ld->blocks[n];
            if (bp->number >= 0)
               if (bp->level < ld->num_refine)
                  bp->refine = 1;
               else
                  bp->refine = 0;
         }
      } else {
         t2 = hpx_time_now();
         check_objects(ld);
         ld->timer_refine_co += hpx_time_elapsed_ms(t2);
         t4 += hpx_time_elapsed_ms(t2);
      }

      t2 = hpx_time_now();
      comm_refine(ld,epoch,count);
      count++;
      comm_parent(ld,epoch,count);
      count++;
      comm_parent_reverse(ld,epoch,count);
      count++;
      ld->timer_refine_c1 += hpx_time_elapsed_ms(t2);
      t4 += hpx_time_elapsed_ms(t2);

      t2 = hpx_time_now();
      num_split = refine_level(ld,epoch,&count);
      t5 = hpx_time_now();
      ld->timer_refine_mr += hpx_time_diff_ms(t5,t2);
      t4 += hpx_time_diff_ms(t5,t2);

      t2 = hpx_time_now();
      sum_b = ld->num_active + 7*num_split + 1;
      hpx_lco_set(ld->refinelevel_max,sizeof(int),&sum_b,HPX_NULL,HPX_NULL);
      hpx_lco_get(ld->refinelevel_max,sizeof(int),&max_b);
      sum_b = ld->num_parents + num_split;
      hpx_lco_set(ld->refinelevel_min,sizeof(int),&sum_b,HPX_NULL,HPX_NULL);
      hpx_lco_get(ld->refinelevel_min,sizeof(int),&min_b);
      if (max_b > ((int) (0.75*((double) ld->max_num_blocks))) ||
          min_b >= (ld->max_num_parents-1)) {
         redistribute_blocks(&tp1, &tm1, &tu1, &t3, &ld->num_moved_rs, num_split,ld,epoch,&count);
         ld->timer_rs_ca += t3;
         ld->nrrs++;
      }
      t5 = hpx_time_now();
      ld->timer_rs_all += hpx_time_diff_ms(t5,t2);
      t4 += hpx_time_diff_ms(t5,t2);
      

   }
}
