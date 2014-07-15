/// ----------------------------------------------------------------------------
/// @file fmm-dag.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief Declare functions for generating the FMM DAG
/// ----------------------------------------------------------------------------
#pragma once 
#ifndef FMM_DAG_H
#define FMM_DAG_H

#include "hpx/hpx.h"
#include "fmm-types.h"

extern hpx_addr_t partition_complete; 
extern fmm_dag_t *fmm_dag_pinned; 
/*

extern fmm_dag_t *fmm_dag; 


fmm_dag_t *construct_dag(const double *sources, int nsources,
			 const double *targets, int ntargets, int s);

// partition_box, build_list13, build_list13_from_box operates on the
// fmm_dag declared in fmm_dag.c 
void partition_box(int level, int index, 
		   const double *points, double h, char tag); 

// destruct_dag and build_merged_list2 operates on any given fmm_dag. 
void destruct_dag(fmm_dag_t *fmm_dag); 

void build_merged_list2(const fmm_dag_t *fmm_dag, 
			disaggr_nonleaf_arg_t *disaggr_nonleaf_arg); 

int is_adjacent(const fmm_box_t *box1, const fmm_box_t *box2); 
*/
#endif 
