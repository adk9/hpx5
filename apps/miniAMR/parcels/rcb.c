
#include "main.h"

void exchange(double *tp, double *tm, double *tu,Domain *ld, unsigned long epoch,int *iter)
{
#if 0
   int f, s, sp, fp, i, j[25], k, l, rb, lev, nfac, block_size, par[25], start[25];
   hpx_time_t t1;
   double t2, t3, t4;

   block_size = 45 + ld->num_vars*ld->x_block_size*ld->y_block_size*ld->z_block_size;

   par[0] = 1;
   lev = 0;
   while (par[lev] < ld->num_pes) {
      par[lev+1] = 2*par[lev];
      lev++;
   }
   j[l = 0] = 0;
   start[0] = 0;
   while(j[0] < 2) {
      if (l == lev) {
         t3 = t4 = 0.0;
         t1 = hpx_time_now();
         sp = fp = s = f = 0;
         // The sense of to and from are reversed in this routine.  They are
         // related to moving the dots from where they ended up back to the
         // processors that they came from and blocks are moved in reverse.
         while (s < from[start[l]] || f < to[start[l]]) {
            if (f < to[start[l]]) {
              if (num_active < max_num_blocks) {
                  rb = 1;
               } else
                  rb = 0;

            }

#endif
}
