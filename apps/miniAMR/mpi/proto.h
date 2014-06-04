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

#include <stddef.h>

// main.c
void print_help_message(void);
void allocate(void);
void deallocate(void);
int check_input(void);

// block.c
void split_blocks(void);
void consolidate_blocks(void);
void add_sorted_list(int, int, int);
void del_sorted_list(int, int);
int find_sorted_list(int, int);

// check_sum.c
double check_sum(int);

// comm_block.c
void comm_proc(void);

// comm.c
void comm(int, int, int);
void pack_face(double *, int, int, int, int, int);
void unpack_face(double *, int, int, int, int, int);
void on_proc_comm(int, int, int, int, int);
void on_proc_comm_diff(int, int, int, int, int, int, int);
void apply_bc(int, block *, int, int);

// comm_parent.c
void comm_parent(void);
void comm_parent_reverse(void);
void comm_parent_unrefine(void);
void comm_parent_proc(void);
void add_par_list(par_comm *, int, int, int, int, int);
void del_par_list(par_comm *, int, int, int, int);

// comm_refine.c
void comm_refine(void);
void comm_reverse_refine(void);
void comm_refine_unrefine(void);

// comm_util.c
void add_comm_list(int, int, int, int, int, int);
void del_comm_list(int, int, int, int);
void zero_comm_list(void);
void check_buff_size(void);
void update_comm_list(void);

// driver.c
void driver(void);

// init.c
void init(void);

// move.c
void move(void);
void check_objects(void);

// pack.c
void pack_block(int);
void unpack_block(int);

// plot.c
void plot(int);

// profile.c
void profile(void);
void calculate_results(void);
void init_profile(void);

// rcb.c
void load_balance(void);
void exchange(double *, double *, double *);
void sort(int, int, int);
int factor(int, int *);
int find_dir(int, int, int, int);
void move_dots(int, int);
void move_dots_back(void);
void move_blocks(double *, double *, double *);

// refine.c
void refine(int);
int refine_level(void);
void reset_all(void);
void reset_neighbors(void);
void redistribute_blocks(double *, double *, double *, double *, int *, int);

// stencil.c
void stencil_calc(int);

// target.c
void reduce_blocks();
void add_blocks();
void zero_refine(void);

// util.c
double timer(void);
void *ma_malloc(size_t, char *, int);
