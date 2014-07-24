
#include "main.h"

void init_profile(Domain *ld)
{
   int i;

   ld->timer_all = 0.0;

   ld->timer_comm_all = 0.0;
   for (i = 0; i < 3; i++) {
      ld->timer_comm_dir[i] = 0.0;
      ld->timer_comm_recv[i] = 0.0;
      ld->timer_comm_pack[i] = 0.0;
      ld->timer_comm_send[i] = 0.0;
      ld->timer_comm_same[i] = 0.0;
      ld->timer_comm_diff[i] = 0.0;
      ld->timer_comm_bc[i] = 0.0;
      ld->timer_comm_wait[i] = 0.0;
      ld->timer_comm_unpack[i] = 0.0;
   }

   ld->timer_calc_all = 0.0;

   ld->timer_cs_all = 0.0;
   ld->timer_cs_red = 0.0;
   ld->timer_cs_calc = 0.0;

   ld->timer_refine_all = 0.0;
   ld->timer_refine_co = 0.0;
   ld->timer_refine_mr = 0.0;
   ld->timer_refine_cc = 0.0;
   ld->timer_refine_sb = 0.0;
   ld->timer_refine_c1 = 0.0;
   ld->timer_refine_c2 = 0.0;
   ld->timer_refine_sy = 0.0;
   ld->timer_cb_all = 0.0;
   ld->timer_cb_cb = 0.0;
   ld->timer_cb_pa = 0.0;
   ld->timer_cb_mv = 0.0;
   ld->timer_cb_un = 0.0;
   ld->timer_target_all = 0.0;
   ld->timer_target_rb = 0.0;
   ld->timer_target_dc = 0.0;
   ld->timer_target_pa = 0.0;
   ld->timer_target_mv = 0.0;
   ld->timer_target_un = 0.0;
   ld->timer_target_cb = 0.0;
   ld->timer_target_ab = 0.0;
   ld->timer_target_da = 0.0;
   ld->timer_target_sb = 0.0;
   ld->timer_lb_all = 0.0;
   ld->timer_lb_sort = 0.0;
   ld->timer_lb_pa = 0.0;
   ld->timer_lb_mv = 0.0;
   ld->timer_lb_un = 0.0;
   ld->timer_lb_misc = 0.0;
   ld->timer_lb_mb = 0.0;
   ld->timer_lb_ma = 0.0;
   ld->timer_rs_all = 0.0;
   ld->timer_rs_ca = 0.0;
   ld->timer_rs_pa = 0.0;
   ld->timer_rs_mv = 0.0;
   ld->timer_rs_un = 0.0;

   ld->timer_plot = 0.0;

   ld->total_blocks = 0;
   ld->nrrs = 0;
   ld->nrs = 0;
   ld->nps = 0;
   ld->nlbs = 0;
   ld->num_refined = 0;
   ld->num_reformed = 0;
   ld->num_moved_all = 0;
   ld->num_moved_lb = 0;
   ld->num_moved_rs = 0;
   ld->num_moved_reduce = 0;
   ld->num_moved_coarsen = 0;
   for (i = 0; i < 3; i++) {
      ld->counter_halo_recv[i] = 0;
      ld->counter_halo_send[i] = 0;
      ld->size_mesg_recv[i] = 0.0;
      ld->size_mesg_send[i] = 0.0;
      ld->counter_face_recv[i] = 0;
      ld->counter_face_send[i] = 0;
      ld->counter_bc[i] = 0;
      ld->counter_same[i] = 0;
      ld->counter_diff[i] = 0;
   }
   ld->total_red = 0;
}

