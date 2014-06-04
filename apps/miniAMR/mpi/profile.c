// ************************************************************************
//
// miniAMR: stencil computations with boundary exchange and AMR.
//
// Copyright (2014) Sandia Corporation. Under the terms of Contract
// DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government 
// retains certain rights in this software.
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
// Questions? Contact Courtenay T. Vaughan (ctvaugh@sandia.gov)
//                    Richard F. Barrett (rfbarre@sandia.gov)
//
// ************************************************************************

#include <stdio.h>
#include <math.h>
#include <mpi.h>

#include "block.h"
#include "comm.h"
#include "proto.h"
#include "timer.h"

// Profiling output.
void profile(void)
{
   int i;
   double total_gflops, gflops_rank, total_fp_ops, total_fp_adds,
          total_fp_divs;
   object *op;

   calculate_results();
   total_fp_divs = ((double) total_blocks)*((double) x_block_size)*
                   ((double) y_block_size)*((double) z_block_size);
   if (stencil == 7)
      total_fp_adds = 6*total_fp_divs;
   else
      total_fp_adds = 26*total_fp_divs;
   total_fp_ops = total_fp_divs + total_fp_adds;
   total_gflops = total_fp_ops/(average[38]*1024.0*1024.0*1024.0);
   gflops_rank = total_gflops/((double) num_pes);

   if (!my_pe) {
//      fp = open("results.yaml" ........
//      fprintf(fp, "code: miniAMR");
//      fprintf(fp, "version: 0.9.5");

      printf("\n ================ Start report ===================\n\n");
      printf("          Mantevo miniAMR\n");
      printf("          version 0.9.5\n\n");

      printf("Run on %d ranks arranged in a %d x %d x %d grid\n", num_pes,
             npx, npy, npz);
      printf("initial blocks per rank %d x %d x %d\n", init_block_x,
             init_block_y, init_block_z);
      printf("block size %d x %d x %d\n", x_block_size, y_block_size,
             z_block_size);
      if (reorder)
         printf("Initial ranks arranged by RCB across machine\n\n");
      else
         printf("Initial ranks arranged as a grid across machine\n\n");
      if (permute)
         printf("Order of exchanges permuted\n");
      printf("Maximum number of blocks per rank is %d\n", max_num_blocks);
      if (target_active)
         printf("Target number of blocks per rank is %d\n", target_active);
      if (target_max)
         printf("Target max number of blocks per rank is %d\n", target_max);
      if (target_min)
         printf("Target min number of blocks per rank is %d\n", target_min);
      if (code)
         printf("Code set to code %d\n", code);
      printf("Number of levels of refinement is %d\n", num_refine);
      printf("Blocks can change by %d levels per refinement step\n",
         block_change);
      if (uniform_refine)
         printf("\nBlocks will be uniformly refined\n");
      else {
         printf("\nBlocks will be refined by %d objects\n\n", num_objects);
         for (i = 0; i < num_objects; i++) {
            op = &objects[i];
            if (op->type == 0)
               printf("Object %d is the surface of a rectangle\n", i);
            else if (op->type == 1)
               printf("Object %d is the volume of a rectangle\n", i);
            else if (op->type == 2)
               printf("Object %d is the surface of a spheroid\n", i);
            else if (op->type == 3)
               printf("Object %d is the volume of a spheroid\n", i);
            else if (op->type == 4)
               printf("Object %d is the surface of x+ hemispheroid\n", i);
            else if (op->type == 5)
               printf("Object %d is the volume of x+ hemispheroid\n", i);
            else if (op->type == 6)
               printf("Object %d is the surface of x- hemispheroid\n", i);
            else if (op->type == 3)
               printf("Object %d is the volume of x- hemispheroid\n", i);
            else if (op->type == 8)
               printf("Object %d is the surface of y+ hemispheroid\n", i);
            else if (op->type == 9)
               printf("Object %d is the volume of y+ hemispheroid\n", i);
            else if (op->type == 10)
               printf("Object %d is the surface of y- hemispheroid\n", i);
            else if (op->type == 11)
               printf("Object %d is the volume of y- hemispheroid\n", i);
            else if (op->type == 12)
               printf("Object %d is the surface of z+ hemispheroid\n", i);
            else if (op->type == 13)
               printf("Object %d is the volume of z+ hemispheroid\n", i);
            else if (op->type == 14)
               printf("Object %d is the surface of z- hemispheroid\n", i);
            else if (op->type == 15)
               printf("Object %d is the volume of z- hemispheroid\n", i);
            else if (op->type == 20)
               printf("Object %d is the surface of x axis cylinder\n", i);
            else if (op->type == 21)
               printf("Object %d is the volune of x axis cylinder\n", i);
            else if (op->type == 22)
               printf("Object %d is the surface of y axis cylinder\n", i);
            else if (op->type == 23)
               printf("Object %d is the volune of y axis cylinder\n", i);
            else if (op->type == 24)
               printf("Object %d is the surface of z axis cylinder\n", i);
            else if (op->type == 25)
               printf("Object %d is the volune of z axis cylinder\n", i);
            if (op->bounce == 0)
               printf("Oject may leave mesh\n");
            else
               printf("Oject center will bounce off of walls\n");
            printf("Center starting at %lf %lf %lf\n",
                   op->orig_cen[0], op->orig_cen[1], op->orig_cen[2]);
            printf("Center end at %lf %lf %lf\n",
                   op->cen[0], op->cen[1], op->cen[2]);
            printf("Moving at %lf %lf %lf per timestep\n",
                   op->orig_move[0], op->orig_move[1], op->orig_move[2]);
            printf("   Rate relative to smallest cell size %lf %lf %lf\n",
                   op->orig_move[0]*((double) (mesh_size[0]*x_block_size)),
                   op->orig_move[1]*((double) (mesh_size[1]*y_block_size)),
                   op->orig_move[2]*((double) (mesh_size[2]*z_block_size)));
            printf("Initial size %lf %lf %lf\n",
                   op->orig_size[0], op->orig_size[1], op->orig_size[2]);
            printf("Final size %lf %lf %lf\n",
                   op->size[0], op->size[1], op->size[2]);
            printf("Size increasing %lf %lf %lf per timestep\n",
                   op->inc[0], op->inc[1], op->inc[2]);
            printf("   Rate relative to smallest cell size %lf %lf %lf\n\n",
                   op->inc[0]*((double) (mesh_size[0]*x_block_size)),
                   op->inc[1]*((double) (mesh_size[1]*y_block_size)),
                   op->inc[2]*((double) (mesh_size[2]*z_block_size)));
         }
      }
      printf("\nNumber of timesteps is %d\n", num_tsteps);
      printf("Communicaion/computation stages per timestep is %d\n",
             stages_per_ts);
      printf("Will perform checksums every %d timesteps\n", checksum_freq);
      printf("Will refine every %d timesteps\n", refine_freq);
      if (lb_opt == 0)
         printf("Load balance will not be performed\n");
      else
         printf("Load balance when inbalanced by %d%\n", inbalance);
      if (lb_opt == 2)
         printf("Load balance at each phase of refinement step\n");
      if (plot_freq)
         printf("Will plot results every %d timesteps\n", plot_freq);
      else
         printf("Will not plot results\n");
      printf("Calculate on %d variables with %d point stencil\n",
             num_vars, stencil);
      printf("Communicate %d variables at a time\n", comm_vars);
      printf("Error tolorance for variable sums is 10^(-%d)\n", error_tol);

      printf("\nTotal time for test: ave, std, min, max (sec): %lf %lf %lf %lf\n\n",
             average[0], stddev[0], minimum[0], maximum[0]);

      printf("\nNumber of malloc calls: ave, std, min, max (sec): %lf %lf %lf %lf\n",
             average[110], stddev[110], minimum[110], maximum[110]);
      printf("\nAmount malloced: ave, std, min, max: %lf %lf %lf %lf\n",
             average[111], stddev[111], minimum[111], maximum[111]);
      printf("\nMalloc calls in init: ave, std, min, max (sec): %lf %lf %lf %lf\n",
             average[112], stddev[112], minimum[112], maximum[112]);
      printf("\nAmount malloced in init: ave, std, min, max: %lf %lf %lf %lf\n",
             average[113], stddev[113], minimum[113], maximum[113]);
      printf("\nMalloc calls in timestepping: ave, std, min, max (sec): %lf %lf %lf %lf\n",
             average[114], stddev[114], minimum[114], maximum[114]);
      printf("\nAmount malloced in timestepping: ave, std, min, max: %lf %lf %lf %lf\n\n",
             average[115], stddev[115], minimum[115], maximum[115]);

      printf("---------------------------------------------\n");
      printf("          Computational Performance\n");
      printf("---------------------------------------------\n\n");
      printf("     Time: ave, stddev, min, max (sec): %lf %lf %lf %lf\n\n",
             average[38], stddev[38], minimum[38], maximum[38]);
      printf("     total GFLOPS:             %lf\n", total_gflops);
      printf("     Average GFLOPS per rank:  %lf\n\n", gflops_rank);
      printf("     Total floating point ops: %lf\n\n", total_fp_ops);
      printf("        Adds:                  %lf\n", total_fp_adds);
      printf("        Divides:               %lf\n\n", total_fp_divs);

      printf("---------------------------------------------\n");
      printf("           Interblock communication\n");
      printf("---------------------------------------------\n\n");
      printf("     Time: ave, stddev, min, max (sec): %lf %lf %lf %lf\n\n",
             average[37], stddev[37], minimum[37], maximum[37]);
      for (i = 0; i < 4; i++) {
         if (i == 0)
            printf("\nTotal communication:\n\n");
         else if (i == 1)
            printf("\nX direction communication statistics:\n\n");
         else if (i == 2)
            printf("\nY direction communication statistics:\n\n");
         else
            printf("\nZ direction communication statistics:\n\n");
         printf("                              average    stddev  minimum  maximum\n");
         printf("     Total                  : %lf %lf %lf %lf\n",
                average[1+9*i], stddev[1+9*i], minimum[1+9*i], maximum[1+9*i]);
         printf("     Post IRecv             : %lf %lf %lf %lf\n",
                average[2+9*i], stddev[2+9*i], minimum[2+9*i], maximum[2+9*i]);
         printf("     Pack faces             : %lf %lf %lf %lf\n",
                average[3+9*i], stddev[3+9*i], minimum[3+9*i], maximum[3+9*i]);
         printf("     Send messages          : %lf %lf %lf %lf\n",
                average[4+9*i], stddev[4+9*i], minimum[4+9*i], maximum[4+9*i]);
         printf("     Exchange same level    : %lf %lf %lf %lf\n",
                average[5+9*i], stddev[5+9*i], minimum[5+9*i], maximum[5+9*i]);
         printf("     Exchange diff level    : %lf %lf %lf %lf\n",
                average[6+9*i], stddev[6+9*i], minimum[6+9*i], maximum[6+9*i]);
         printf("     Apply BC               : %lf %lf %lf %lf\n",
                average[7+9*i], stddev[7+9*i], minimum[7+9*i], maximum[7+9*i]);
         printf("     Wait time              : %lf %lf %lf %lf\n",
                average[8+9*i], stddev[8+9*i], minimum[8+9*i], maximum[8+9*i]);
         printf("     Unpack faces           : %lf %lf %lf %lf\n\n",
                average[9+9*i], stddev[9+9*i], minimum[9+9*i], maximum[9+9*i]);

         printf("     Messages received      : %lf %lf %lf %lf\n",
            average[70+9*i], stddev[70+9*i], minimum[70+9*i], maximum[70+9*i]);
         printf("     Bytes received         : %lf %lf %lf %lf\n",
            average[68+9*i], stddev[68+9*i], minimum[68+9*i], maximum[68+9*i]);
         printf("     Faces received         : %lf %lf %lf %lf\n",
            average[72+9*i], stddev[72+9*i], minimum[72+9*i], maximum[72+9*i]);
         printf("     Messages sent          : %lf %lf %lf %lf\n",
            average[71+9*i], stddev[71+9*i], minimum[71+9*i], maximum[71+9*i]);
         printf("     Bytes sent             : %lf %lf %lf %lf\n",
            average[69+9*i], stddev[69+9*i], minimum[69+9*i], maximum[69+9*i]);
         printf("     Faces sent             : %lf %lf %lf %lf\n",
            average[73+9*i], stddev[73+9*i], minimum[73+9*i], maximum[73+9*i]);
         printf("     Faces exchanged same   : %lf %lf %lf %lf\n",
            average[75+9*i], stddev[75+9*i], minimum[75+9*i], maximum[75+9*i]);
         printf("     Faces exchanged diff   : %lf %lf %lf %lf\n",
            average[76+9*i], stddev[76+9*i], minimum[76+9*i], maximum[76+9*i]);
         printf("     Faces with BC applied  : %lf %lf %lf %lf\n",
            average[74+9*i], stddev[74+9*i], minimum[74+9*i], maximum[74+9*i]);
      }

      printf("\n---------------------------------------------\n");
      printf("             Gridsum performance\n");
      printf("---------------------------------------------\n\n");
      printf("     Time: ave, stddev, min, max (sec): %lf %lf %lf %lf\n\n",
             average[39], stddev[39], minimum[39], maximum[39]);
      printf("        red : ave, stddev, min, max (sec): %lf %lf %lf %lf\n\n",
             average[40], stddev[40], minimum[40], maximum[40]);
      printf("        calc: ave, stddev, min, max (sec): %lf %lf %lf %lf\n\n",
             average[41], stddev[41], minimum[41], maximum[41]);
      printf("     total number:             %d\n", total_red);
      printf("     number per timestep:      %d\n\n", num_vars);

      printf("---------------------------------------------\n");
      printf("               Mesh Refinement\n");
      printf("---------------------------------------------\n\n");
      printf("     Time: ave, stddev, min, max (sec): %lf %lf %lf %lf\n\n",
             average[42], stddev[42], minimum[42], maximum[42]);
      printf("     Number of refinement steps: %d\n\n", nrs);
      printf("     Number of load balance steps: %d\n\n", nlbs);
      printf("     Number of redistributing steps: %d\n\n", nrrs);
      printf("     Total blocks           : %ld\n", total_blocks);
      printf("     Blocks/timestep ave, min, max : %lf %d %d\n",
             ((double) total_blocks)/((double) (num_tsteps*stages_per_ts)),
             nb_min, nb_max);
      printf("     Max blocks on a processor at any time: %d\n", global_max_b);
      printf("     total blocks split     : %lf\n", average[104]*num_pes);
      printf("     total blocks reformed  : %lf\n\n", average[105]*num_pes);
      printf("     total blocks moved     : %lf\n", average[106]*num_pes);
      printf("     total moved load bal   : %lf\n", average[107]*num_pes);
      printf("     total moved redistribut: %lf\n", average[122]*num_pes);
      printf("     total moved coasening  : %lf\n", average[109]*num_pes);
      if (target_active)
         printf("     total moved reducing   : %lf\n", average[108]*num_pes);
      printf("                              average    stddev  minimum  maximum\n");
      printf("     Per processor:\n");
      printf("     total blocks split     : %lf %lf %lf %lf\n",
             average[104], stddev[104], minimum[104], maximum[104]);
      printf("     total blocks reformed  : %lf %lf %lf %lf\n",
             average[105], stddev[105], minimum[105], maximum[105]);
      printf("     Total blocks moved     : %lf %lf %lf %lf\n",
             average[106], stddev[106], minimum[106], maximum[106]);
      printf("     Blocks moved load bal  : %lf %lf %lf %lf\n",
             average[107], stddev[107], minimum[107], maximum[107]);
      printf("     Blocks moved redistribu: %lf %lf %lf %lf\n",
             average[122], stddev[122], minimum[122], maximum[122]);
      printf("     Blocks moved coarsening: %lf %lf %lf %lf\n",
             average[109], stddev[109], minimum[109], maximum[109]);
      if (target_active)
         printf("     Blocks moved reducing  : %lf %lf %lf %lf\n",
                average[108], stddev[108], minimum[108], maximum[108]);
      printf("     Time:\n");
      printf("        compare objects     : %lf %lf %lf %lf\n",
             average[43], stddev[43], minimum[43], maximum[43]);
      printf("        mark refine/coarsen : %lf %lf %lf %lf\n",
             average[44], stddev[44], minimum[44], maximum[44]);
      printf("        communicate block 1 : %lf %lf %lf %lf\n",
             average[119], stddev[119], minimum[119], maximum[119]);
      printf("        split blocks        : %lf %lf %lf %lf\n",
             average[46], stddev[46], minimum[46], maximum[46]);
      printf("        communicate block 2 : %lf %lf %lf %lf\n",
             average[120], stddev[120], minimum[120], maximum[120]);
      printf("        sync time           : %lf %lf %lf %lf\n",
             average[121], stddev[121], minimum[121], maximum[121]);
      printf("        misc time           : %lf %lf %lf %lf\n",
             average[45], stddev[45], minimum[45], maximum[45]);
      printf("        total coarsen blocks: %lf %lf %lf %lf\n",
             average[47], stddev[47], minimum[47], maximum[47]);
      printf("           coarsen blocks   : %lf %lf %lf %lf\n",
             average[48], stddev[48], minimum[48], maximum[48]);
      printf("           pack blocks      : %lf %lf %lf %lf\n",
             average[49], stddev[49], minimum[49], maximum[49]);
      printf("           move blocks      : %lf %lf %lf %lf\n",
             average[50], stddev[50], minimum[50], maximum[50]);
      printf("           unpack blocks    : %lf %lf %lf %lf\n",
             average[51], stddev[51], minimum[51], maximum[51]);
      printf("        total redistribute  : %lf %lf %lf %lf\n",
             average[123], stddev[123], minimum[123], maximum[123]);
      printf("           choose blocks    : %lf %lf %lf %lf\n",
             average[124], stddev[124], minimum[124], maximum[124]);
      printf("           pack blocks      : %lf %lf %lf %lf\n",
             average[125], stddev[125], minimum[125], maximum[125]);
      printf("           move blocks      : %lf %lf %lf %lf\n",
             average[126], stddev[126], minimum[126], maximum[126]);
      printf("           unpack blocks    : %lf %lf %lf %lf\n",
             average[127], stddev[127], minimum[127], maximum[127]);
      if (target_active) {
         printf("        total target active : %lf %lf %lf %lf\n",
             average[52], stddev[52], minimum[52], maximum[52]);
         printf("          reduce blocks     : %lf %lf %lf %lf\n",
             average[53], stddev[53], minimum[53], maximum[53]);
         printf("            decide and comm : %lf %lf %lf %lf\n",
             average[54], stddev[54], minimum[54], maximum[54]);
         printf("            pack blocks     : %lf %lf %lf %lf\n",
             average[55], stddev[55], minimum[55], maximum[55]);
         printf("            move blocks     : %lf %lf %lf %lf\n",
             average[56], stddev[56], minimum[56], maximum[56]);
         printf("            unpack blocks   : %lf %lf %lf %lf\n",
             average[57], stddev[57], minimum[57], maximum[57]);
         printf("            coarsen blocks  : %lf %lf %lf %lf\n",
             average[58], stddev[58], minimum[58], maximum[58]);
         printf("          add blocks        : %lf %lf %lf %lf\n",
             average[59], stddev[59], minimum[59], maximum[59]);
         printf("            decide and comm : %lf %lf %lf %lf\n",
             average[60], stddev[60], minimum[60], maximum[60]);
         printf("            split blocks    : %lf %lf %lf %lf\n",
             average[61], stddev[61], minimum[61], maximum[61]);
      }
      printf("        total load balance  : %lf %lf %lf %lf\n",
             average[62], stddev[62], minimum[62], maximum[62]);
      printf("           sort             : %lf %lf %lf %lf\n",
             average[63], stddev[63], minimum[63], maximum[63]);
      printf("           move dots back   : %lf %lf %lf %lf\n",
             average[117], stddev[117], minimum[117], maximum[117]);
      printf("           move blocks total: %lf %lf %lf %lf\n",
             average[118], stddev[118], minimum[118], maximum[118]);
      printf("              pack blocks   : %lf %lf %lf %lf\n",
             average[64], stddev[64], minimum[64], maximum[64]);
      printf("              move blocks   : %lf %lf %lf %lf\n",
             average[65], stddev[65], minimum[65], maximum[65]);
      printf("              unpack blocks : %lf %lf %lf %lf\n",
             average[66], stddev[66], minimum[66], maximum[66]);
      printf("              misc          : %lf %lf %lf %lf\n\n",
             average[116], stddev[116], minimum[116], maximum[116]);

      printf("---------------------------------------------\n");
      printf("                   Plot\n");
      printf("---------------------------------------------\n\n");
      printf("     Time: ave, stddev, min, max (sec): %lf %lf %lf %lf\n\n",
             average[67], stddev[67], minimum[67], maximum[67]);
      printf("     Number of plot steps: %d\n", nps);
      printf("\n ================== End report ===================\n");
   }
}

void calculate_results(void)
{
   double results[128], stddev_sum[128];
   int i;

   results[0] = timer_all;
   for (i = 0; i < 9; i++)
      results[i+1] = 0.0;
   for (i = 0; i < 3; i++) {
      results[1] += results[10+9*i] = timer_comm_dir[i];
      results[2] += results[11+9*i] = timer_comm_recv[i];
      results[3] += results[12+9*i] = timer_comm_pack[i];
      results[4] += results[13+9*i] = timer_comm_send[i];
      results[5] += results[14+9*i] = timer_comm_same[i];
      results[6] += results[15+9*i] = timer_comm_diff[i];
      results[7] += results[16+9*i] = timer_comm_bc[i];
      results[8] += results[17+9*i] = timer_comm_wait[i];
      results[9] += results[18+9*i] = timer_comm_unpack[i];
   }
   results[37] = timer_comm_all;
   results[38] = timer_calc_all;
   results[39] = timer_cs_all;
   results[40] = timer_cs_red;
   results[41] = timer_cs_calc;
   results[42] = timer_refine_all;
   results[43] = timer_refine_co;
   results[44] = timer_refine_mr;
   results[45] = timer_refine_cc;
   results[46] = timer_refine_sb;
   results[119] = timer_refine_c1;
   results[120] = timer_refine_c2;
   results[121] = timer_refine_sy;
   results[47] = timer_cb_all;
   results[48] = timer_cb_cb;
   results[49] = timer_cb_pa;
   results[50] = timer_cb_mv;
   results[51] = timer_cb_un;
   results[52] = timer_target_all;
   results[53] = timer_target_rb;
   results[54] = timer_target_dc;
   results[55] = timer_target_pa;
   results[56] = timer_target_mv;
   results[57] = timer_target_un;
   results[58] = timer_target_cb;
   results[59] = timer_target_ab;
   results[60] = timer_target_da;
   results[61] = timer_target_sb;
   results[62] = timer_lb_all;
   results[63] = timer_lb_sort;
   results[64] = timer_lb_pa;
   results[65] = timer_lb_mv;
   results[66] = timer_lb_un;
   results[116] = timer_lb_misc;
   results[117] = timer_lb_mb;
   results[118] = timer_lb_ma;
   results[67] = timer_plot;
   results[123] = timer_rs_all;
   results[124] = timer_rs_ca;
   results[125] = timer_rs_pa;
   results[126] = timer_rs_mv;
   results[127] = timer_rs_un;
   for (i = 0; i < 9; i++)
      results[68+i] = 0.0;
   for (i = 0; i < 3; i++) {
      results[68] += results[77+9*i] = size_mesg_recv[i];
      results[69] += results[78+9*i] = size_mesg_send[i];
      results[70] += results[79+9*i] = (double) counter_halo_recv[i];
      results[71] += results[80+9*i] = (double) counter_halo_send[i];
      results[72] += results[81+9*i] = (double) counter_face_recv[i];
      results[73] += results[82+9*i] = (double) counter_face_send[i];
      results[74] += results[83+9*i] = (double) counter_bc[i];
      results[75] += results[84+9*i] = (double) counter_same[i];
      results[76] += results[85+9*i] = (double) counter_diff[i];
   }
   results[104] = (double) num_refined;
   results[105] = (double) num_reformed;
   num_moved_all = num_moved_lb + num_moved_reduce + num_moved_coarsen +
                   num_moved_rs;
   results[106] = (double) num_moved_all;
   results[107] = (double) num_moved_lb;
   results[122] = (double) num_moved_rs;
   results[108] = (double) num_moved_reduce;
   results[109] = (double) num_moved_coarsen;
   results[110] = (double) counter_malloc;
   results[111] = size_malloc;
   results[112] = (double) counter_malloc_init;
   results[113] = size_malloc_init;
   results[114] = (double) (counter_malloc - counter_malloc_init);
   results[115] = size_malloc - size_malloc_init;

   MPI_Allreduce(results, average, 128, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
   MPI_Allreduce(results, minimum, 128, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
   MPI_Allreduce(results, maximum, 128, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);

   for (i = 0; i < 128; i++) {
      average[i] /= (double) num_pes;
      stddev[i] = (results[i] - average[i])*(results[i] - average[i]);
   }
   MPI_Allreduce(stddev, stddev_sum, 128, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
   for (i = 0; i < 128; i++)
      stddev[i] = sqrt(stddev_sum[i]/((double) num_pes));
}

void init_profile(void)
{
   int i;

   timer_all = 0.0;

   timer_comm_all = 0.0;
   for (i = 0; i < 3; i++) {
      timer_comm_dir[i] = 0.0;
      timer_comm_recv[i] = 0.0;
      timer_comm_pack[i] = 0.0;
      timer_comm_send[i] = 0.0;
      timer_comm_same[i] = 0.0;
      timer_comm_diff[i] = 0.0;
      timer_comm_bc[i] = 0.0;
      timer_comm_wait[i] = 0.0;
      timer_comm_unpack[i] = 0.0;
   }

   timer_calc_all = 0.0;

   timer_cs_all = 0.0;
   timer_cs_red = 0.0;
   timer_cs_calc = 0.0;

   timer_refine_all = 0.0;
   timer_refine_co = 0.0;
   timer_refine_mr = 0.0;
   timer_refine_cc = 0.0;
   timer_refine_sb = 0.0;
   timer_refine_c1 = 0.0;
   timer_refine_c2 = 0.0;
   timer_refine_sy = 0.0;
   timer_cb_all = 0.0;
   timer_cb_cb = 0.0;
   timer_cb_pa = 0.0;
   timer_cb_mv = 0.0;
   timer_cb_un = 0.0;
   timer_target_all = 0.0;
   timer_target_rb = 0.0;
   timer_target_dc = 0.0;
   timer_target_pa = 0.0;
   timer_target_mv = 0.0;
   timer_target_un = 0.0;
   timer_target_cb = 0.0;
   timer_target_ab = 0.0;
   timer_target_da = 0.0;
   timer_target_sb = 0.0;
   timer_lb_all = 0.0;
   timer_lb_sort = 0.0;
   timer_lb_pa = 0.0;
   timer_lb_mv = 0.0;
   timer_lb_un = 0.0;
   timer_lb_misc = 0.0;
   timer_lb_mb = 0.0;
   timer_lb_ma = 0.0;
   timer_rs_all = 0.0;
   timer_rs_ca = 0.0;
   timer_rs_pa = 0.0;
   timer_rs_mv = 0.0;
   timer_rs_un = 0.0;

   timer_plot = 0.0;

   total_blocks = 0;
   nrrs = 0;
   nrs = 0;
   nps = 0;
   nlbs = 0;
   num_refined = 0;
   num_reformed = 0;
   num_moved_all = 0;
   num_moved_lb = 0;
   num_moved_rs = 0;
   num_moved_reduce = 0;
   num_moved_coarsen = 0;
   for (i = 0; i < 3; i++) {
      counter_halo_recv[i] = 0;
      counter_halo_send[i] = 0;
      size_mesg_recv[i] = 0.0;
      size_mesg_send[i] = 0.0;
      counter_face_recv[i] = 0;
      counter_face_send[i] = 0;
      counter_bc[i] = 0;
      counter_same[i] = 0;
      counter_diff[i] = 0;
   }
   total_red = 0;
}
