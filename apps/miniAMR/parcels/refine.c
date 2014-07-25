
#include "main.h"

void refine(int ts,Domain *ld,unsigned long epoch)
{
  int i, j, n, p, c, in, min_b, max_b, sum_b, num_refine_step, num_split;
   double ratio, tp, tm, tu, tp1, tm1, tu1, t1, t3, t4, t5;
   block *bp;
   parent *pp;

   ld->nrs++;
   t4 = tp = tm = tu = tp1 = tm1 = tu1 = 0.0;

   hpx_time_t t2 = hpx_time_now();

   hpx_lco_set(ld->rsum,(ld->num_refine+1)*sizeof(int),ld->local_num_blocks,HPX_NULL,HPX_NULL);
   hpx_lco_get(ld->rsum,(ld->num_refine+1)*sizeof(int),ld->num_blocks);

 //  MPI_Allreduce(local_num_blocks, num_blocks, (num_refine+1), MPI_INTEGER,
 //                MPI_SUM, MPI_COMM_WORLD);

   ld->timer_refine_sy += hpx_time_elapsed_ms(t2);
   t4 += hpx_time_elapsed_ms(t2);
}
