#pragma once
#ifndef FMM_TYPES_H
#define FMM_TYPES_H

#include <complex.h>
#include "hpx/hpx.h"
#define MAXLEV 128

typedef struct {
  int nsources;
  int ntargets;
  int datatype;
  int accuracy;
  int s;
} fmm_config_t;

typedef struct {
  int level;
  int boxid;
  int parent;
  int child[8];
  int nchild;
  int idx;
  int idy;
  int idz;
  int npts;
  int addr;
  int *list1;
  int *list3;
  int *list4;
  int *list5;
} fmm_box_t;

typedef struct {
  int nslev;
  int nsboxes;
  int ntlev;
  int ntboxes;
  double size;
  double corner[3];
  int *mapsrc;
  int *maptar;
  fmm_box_t *sboxes[MAXLEV];
  fmm_box_t *tboxes[MAXLEV];
  fmm_box_t **sboxptrs;
  fmm_box_t **tboxptrs;
} fmm_dag_t;

typedef struct {
  int pterms;
  int nlambs;
  int pgsz;
  int nexptot;
  int nthmax;
  int nexptotp;
  int nexpmax;
  int *numphys;
  int *numfour;

  double *whts;
  double *rlams;
  double *rdplus;
  double *rdminus;
  double *rdsq3;
  double *rdmsq3;
  double *dc;
  double *ytopc;
  double *ytopcs;
  double *ytopcsinv;
  double *rlsc;
  double *zs;

  double complex *xs;
  double complex *ys;
  double complex *fexpe;
  double complex *fexpo;
  double complex *fexpback;
} fmm_param_t;

typedef struct {
  int boxid;
  int nsources;
  double scale;
  double center[3];
  double points[];
} aggr_leaf_arg_t;

typedef struct {
  int boxid;
  int child[8];
} aggr_nonleaf_arg_t;

typedef struct {
  int parent;
  int boxid;
  int which;
  int level;
  int child[8];
  int uall[36], nuall, xuall[36], yuall[36];
  int u1234[16], nu1234, x1234[16], y1234[16];
  int dall[36], ndall, xdall[36], ydall[36];
  int d5678[16], nd5678, x5678[16], y5678[16];
  int nall[24], nnall, xnall[24], ynall[24];
  int n1256[8], nn1256, x1256[8], y1256[8];
  int n12[4], nn12, x12[4], y12[4];
  int n56[4], nn56, x56[4], y56[4];
  int sall[24], nsall, xsall[24], ysall[24];
  int s3478[8], ns3478, x3478[8], y3478[8];
  int s34[4], ns34, x34[4], y34[4];
  int s78[4], ns78, x78[4], y78[4];
  int eall[16], neall, xeall[16], yeall[16];
  int e1357[4], ne1357, x1357[4], y1357[4];
  int e13[2], ne13, x13[2], y13[2];
  int e57[2], ne57, x57[2], y57[2];
  int e1[3], ne1, x1[3], y1[3];
  int e3[3], ne3, x3[3], y3[3];
  int e5[3], ne5, x5[3], y5[3];
  int e7[3], ne7, x7[3], y7[3];
  int wall[16], nwall, xwall[16], ywall[16];
  int w2468[4], nw2468, x2468[4], y2468[4];
  int w24[2], nw24, x24[2], y24[2];
  int w68[2], nw68, x68[2], y68[2];
  int w2[3], nw2, x2[3], y2[3];
  int w4[3], nw4, x4[3], y4[3];
  int w6[3], nw6, x6[3], y6[3];
  int w8[3], nw8, x8[3], y8[3];
  double scale;
} disaggr_nonleaf_arg_t;

typedef struct {
  int parent;
  int boxid;
  int which;
  int ntargets;
  double scale;
  double center[3];
  double points[];
} disaggr_leaf_arg_t;

typedef struct {
  int boxid; 
  double result[]; 
} fmm_result_arg_t; 

typedef struct {
  hpx_addr_t mpole; 
  hpx_addr_t local_h;
  hpx_addr_t local_v;
  hpx_addr_t expu;
  hpx_addr_t expd;
  hpx_addr_t expn;
  hpx_addr_t exps;
  hpx_addr_t expe;
  hpx_addr_t expw;
} fmm_expan_arg_t; 

#endif
