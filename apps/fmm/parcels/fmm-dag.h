#pragma once 
#ifndef FMM_DAG_H
#define FMM_DAG_H

#include "fmm-types.h"

fmm_dag_t *construct_dag(const double *sources, int nsources,
			 const double *targets, int ntargets, int s);

// partition_box, build_list13, build_list13_from_box operates on the
// fmm_dag declared in fmm_dag.c 
void partition_box(int level, int index, 
		   const double *points, double h, char tag); 

void build_list13(int boxid, const int *coarse_list, int ncoarse_list); 

void build_list13_from_box(fmm_box_t *tbox, fmm_box_t *sbox, int *list1, 
			   int *nlist1, int *list3, int *nlist3);

// destruct_dag and build_merged_list2 operates on any given fmm_dag. 
void destruct_dag(fmm_dag_t *fmm_dag); 

void build_merged_list2(const fmm_dag_t *fmm_dag, 
			disaggr_nonleaf_arg_t *disaggr_nonleaf_arg); 

int is_adjacent(const fmm_box_t *box1, const fmm_box_t *box2); 
#endif 
