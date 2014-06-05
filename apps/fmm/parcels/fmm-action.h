#pragma once
#ifndef FMM_ACTION_H
#define FMM_ACTION_H

#include "hpx/hpx.h"

int _fmm_main_action(void *args); 

int _fmm_build_list134_action(void *args); 

int _fmm_bcast_action(void *args); 

int _aggr_leaf_sbox_action(void *args); 

int _aggr_nonleaf_sbox_action(void *args); 

int _disaggr_leaf_tbox_action(void *args); 

int _disaggr_nonleaf_tbox_action(void *args); 

int _recv_result_action(void *args); 

int _process_near_field_action(void *args); 

hpx_action_t _fmm_main; 

hpx_action_t _fmm_build_list134; 

hpx_action_t _fmm_bcast; 

hpx_action_t _aggr_leaf_sbox;  

hpx_action_t _aggr_nonleaf_sbox; 

hpx_action_t _disaggr_leaf_tbox; 

hpx_action_t _disaggr_nonleaf_tbox; 

hpx_action_t _send_result; 

hpx_action_t _recv_result; 

hpx_action_t _process_near_field; 

void source_to_multipole(const double *points, int nsources, 
			 const double *center, double scale, 
			 double complex *multipole); 

void multipole_to_multipole(const double complex *cmpole, int ichild, 
			    double complex *pmpole); 

void multipole_to_exponential(const double complex *multipole,
                              double complex *mexpu,
                              double complex *mexpd);

void exponential_to_local(const disaggr_nonleaf_arg_t *disaggr_nonleaf_arg, 
			  const int child[8], double complex *clocal);

void exponential_to_local_p1(const double complex *mexpphys, 
			     double complex *mexpf); 

void exponential_to_local_p2(int iexpu, const double complex *mexpu, 
			     int iexpd, const double complex *mexpd, 
			     double complex *local); 

void local_to_local(const double complex *plocal, int which, 
		    double complex *local); 

void local_to_target(const double complex *local, const double *center, 
		     double scale, const double *points, 
		     int ntargets, double *result); 
		    
void process_list4(const fmm_dag_t *fmm_dag, int boxid, 
		   const double *sources, const double *charges, 
		   const double *targets, double *potential, double *field); 

void process_list13(const fmm_dag_t *fmm_dag, int boxid, 
		    const double *sources, const double *charges, 
		    const double *targets, double *potential, double *field); 

void direct_evaluation(const double *sources, const double *charges, 
		       int nsources, const double *targets, int ntargets, 
		       double *potential, double *field); 

void lgndr(int nmax, double x, double *y); 

void rotz2y(const double complex *multipole, const double *rd,
            double complex *mrotate);

void roty2z(const double complex *multipole, const double *rd,
            double complex *mrotate);

void rotz2x(const double complex *multipole, const double *rd,
            double complex *mrotate);

void make_ulist(hpx_addr_t expo, const int *list, int nlist,
		const int *xoff, const int *yoff, double complex *mexpo);

void make_dlist(hpx_addr_t expo, const int *list, int nlist,
		const int *xoff, const int *yoff, double complex *mexpo);
#endif
