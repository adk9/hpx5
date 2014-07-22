#ifndef LULESH_HPX_H
#define LULESH_HPX_H

#include <limits.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "hpx/hpx.h"

typedef struct {
   int type;
   int bounce;
   double cen[3];
   double orig_cen[3];
   double move[3];
   double orig_move[3];
   double size[3];
   double orig_size[3];
   double inc[3];
} object;

typedef struct {
  hpx_addr_t complete;
  int params[34];
  int objectsize;
  int rank;
  int ndoms;
  object objects[];
} InitArgs;

typedef struct {
  int *params;
  int paramsize;
  int objectsize;
  object objects[];
} RunArgs;

static inline size_t RunArgs_size(RunArgs *runargs) {
  assert(runargs != NULL);
  return sizeof(*runargs) + (sizeof(object) * runargs->objectsize);
}

typedef struct {
   int number;     // number of block
   int n;          // position in block array
} sorted_block;

typedef struct {
   int number;
   int level;
   int refine;
   int new_proc;
   int parent;       // if original block -1,
                     // else if on node, number in structure
                     // else (-2 - parent->number)
   int parent_node;
   int child_number;
   int nei_refine[6];
   int nei_level[6];  /* 0 to 5 = W, E, S, N, D, U; use -2 for boundary */
   int nei[6][2][2];  /* negative if off processor (-1 - proc) */
   int cen[3];
   double ****array;
} block;

typedef struct {
   int number;
   int level;
   int parent;      // -1 if original block
   int parent_node;
   int child_number;
   int refine;
   int child[8];    // n if on node, number if not
                    // if negative, then onnode child is a parent (-1 - n)
   int child_node[8];
   int cen[3];
} parent;

typedef struct {
   int cen[3];
   int number;
   int n;
   int proc;
   int new_proc;
} dot;

typedef struct {
   int num_comm_part;          // number of other cores to communicate with
   int *comm_part;             // core to communicate with
   int *comm_num;              // number to communicate to each core
   int *index;                 // offset into next two arrays
   int *comm_b;                // block number to communicate from
   int *comm_p;                // parent number of block (for sorting)
   int *comm_c;                // child number of block
   int max_part;               // max communication partners
   int num_cases;              // number to communicate
   int max_cases;              // max number to communicate
} par_comm;

typedef struct Domain {
  int ts;
  int max_num_blocks;
  int target_active;
  int num_refine;
  int uniform_refine;
  int x_block_size;
  int y_block_size;
  int z_block_size;
  int num_vars;
  int comm_vars;
  int init_block_x;
  int init_block_y;
  int init_block_z;
  int reorder;
  int npx;
  int npy;
  int npz;
  int inbalance;
  int refine_freq;
  int report_diffusion;
  int error_tol;
  double tol;
  int num_tsteps;
  int stencil;
  int report_perf;
  int plot_freq;
  int num_objects;
  int checksum_freq;
  int target_max;
  int target_min;
  int stages_per_ts;
  int lb_opt;
  int block_change;
  int code;
  int permute;
  int num_pes;
  hpx_addr_t complete;
  int *num_blocks;
  int *local_num_blocks;
  block *blocks;
  int *sorted_index;
  sorted_block *sorted_list;
  int max_num_parents;
  parent *parents;
  int max_num_dots;
  dot *dots;
  double *grid_sum;
  int *p8, *p2;
  int *block_start;
  int *from, *to;

  int num_comm_partners[3],  // number of comm partners in each direction
    *comm_partner[3],      // list of comm partners in each direction
    max_comm_part[3],      // lengths of comm partners arrays
    *send_size[3],         // send sizes for each comm partner
    *recv_size[3],         // recv sizes for each comm partner
    *comm_index[3],        // index into comm_block, _face_case, and offsets
    *comm_num[3],          // number of blocks for each comm partner
    *comm_block[3],        // array containing local block number for comm
    *comm_face_case[3],    // array containing face cases for comm
    *comm_pos[3],          // position for center of sending face
    *comm_pos1[3],         // perpendicular position of sending face
    *comm_send_off[3],     // offset into send buffer (global, convert to local)
    *comm_recv_off[3],     // offset into recv buffer
    num_cases[3],          // amount used in above six arrays
    max_num_cases[3],      // length of above six arrays
    s_buf_num[3],          // total amount being sent in each direction
    r_buf_num[3];          // total amount being received in each direction
    par_comm par_b, par_p, par_p1;
  int s_buf_size, r_buf_size;
  double *send_buff, *recv_buff;    /* use in comm and for balancing blocks */

  int global_max_b;
  int x_block_half, y_block_half, z_block_half;
  int msg_len[3][4];
  int max_num_req;
  int *me;
  int *np;
  int max_active_block;
  int num_active;
  int global_active;
  int num_parents;
  int mesh_size[3];
  int max_mesh_size;
  int *bin;
  int *gbin;
  int my_pe;
  int max_active_parent;
} Domain;

#endif
