#ifndef FMM_H
#define FMM_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <complex.h>
#include "hpx.h"

#define MAXLEV 128

typedef struct Box {
  int level, boxid, parent, child[8], nchild, idx, idy, idz, npts, addr,
    *list1, *list3, *list4, *list5;
} Box; 

int nsources_, ntargets_; 
double *sources_, *charges_, *targets_, *potential_, *field_; 
int nslev_, nsboxes_, *mapsrc_, ntlev_, ntboxes_, *maptar_; 
double size_, corner_[3]; 
Box *sboxes_[MAXLEV], *tboxes_[MAXLEV], **sboxptrs_, **tboxptrs_; 

static const int iflu_[8] = {3, 4, 2, 1, 3, 4, 2, 1};
static const int ifld_[8] = {1, 2, 4, 3, 1, 2, 4, 3}; 

int pterms_, nlambs_, pgsz_; 
int *numphys_, *numfour_, nexptot_, nthmax_, nexptotp_, nexpmax_; 
double *whts_, *rlams_, *rdplus_, *rdminus_, *rdsq3_, *rdmsq3_, 
  *dc_, *ytopc_, *ytopcs_, *ytopcsinv_, *rlsc_, *zs_, *scale_; 
double complex *xs_, *ys_, *fexpe_, *fexpo_, *fexpback_, 
  *mpole_, *local_, *expu_, *expd_, *expn_, *exps_, *expe_, *expw_; 

void BuildGraph(const double *sources, int nsources, 
		const double *targets, int ntargets, int s); 

void PartitionBox(Box *ibox, const double *points, double h, char tag);

void BuildList134(Box *tbox);

void BuildList13(Box *tbox, const int *coarse_list, int ncoarse_list);

void BuildList13FromBox (Box *tbox, Box *sbox, int *list1, int *nlist1, 
			 int *list3, int *nlist3);

int IsAdjacent(const Box *box1, const Box *box2);

void BuildMergedList2(const Box *ibox, 
		      int *uall, int *nuall, int *xuall, int *yuall, 
		      int *u1234, int *nu1234, int *x1234, int *y1234, 
		      int *dall, int *ndall, int *xdall, int *ydall,
		      int *d5678, int *nd5678, int *x5678, int *y5678,
		      int *nall, int *nnall, int *xnall, int *ynall,
		      int *n1256, int *nn1256, int *x1256, int *y1256,
		      int *n12, int *nn12, int *x12, int *y12, 
		      int *n56, int *nn56, int *x56, int *y56, 
		      int *sall, int *nsall, int *xsall, int *ysall,
		      int *s3478, int *ns3478, int *x3478, int *y3478,
		      int *s34, int *ns34, int *x34, int *y34, 
		      int *s78, int *ns78, int *x78, int *y78, 
		      int *eall, int *neall, int *xeall, int *yeall,
		      int *e1357, int *ne1357, int *x1357, int *y1357,
		      int *e13, int *ne13, int *x13, int *y13, 
		      int *e57, int *ne57, int *x57, int *y57, 
		      int *e1, int *ne1, int *x1, int *y1,
		      int *e3, int *ne3, int *x3, int *y3, 
		      int *e5, int *ne5, int *x5, int *y5, 
		      int *e7, int *ne7, int *x7, int *y7, 
		      int *wall, int *nwall, int *xwall, int *ywall, 
		      int *w2468, int *nw2468, int *x2468, int *y2468, 
		      int *w24, int *nw24, int *x24, int *y24, 
		      int *w68, int *nw68, int *x68, int *y68, 
		      int *w2, int *nw2, int *x2, int *y2, 
		      int *w4, int *nw4, int *x4, int *y4, 
		      int *w6, int *nw6, int *x6, int *y6, 
		      int *w8, int *nw8, int *x8, int *y8);

static inline void UpdateList(int *list, int *nlist, int *xoff, int *yoff, 
			      int boxid, int ix, int iy)
{
  list[*nlist] = boxid; 
  xoff[*nlist] = ix; 
  yoff[*nlist] = iy; 
  (*nlist)++; 
}

void DestroyGraph(void); 

void frmini(void);

void rotgen(void);

void bnlcft(double *c, int p);

void fstrtn(int p, double *d, const double *sqc, double theta);

void vwts(void); 

void numthetahalf(void);

void numthetafour(void);

void rlscini(void);

void mkfexp(void);

void mkexps(void); 

void lgndr(int nmax, double x, double *y);

void FMMCompute(void); 

void ComputeMultipole(void *data);

void ComputeExponential(void *data);

void ComputeLocal(void *data);

void SourceToMultipole(const Box *ibox); 

void MultipoleToMultipole(const Box *pbox);

void MultipoleToExponential(const Box *ibox);

void MultipoleToExponentialPhase1(const double complex *multipole, 
				  double complex *mexpu, double complex *mexpd);

void MultipoleToExponentialPhase2(const double complex *mexpf, double complex *mexpphys);

void ExponentialToLocal(const Box *ibox);

void ExponentialToLocalPhase1(const double complex *mexpphys, double complex *mexpf);

void ExponentialToLocalPhase2(int iexpu, const double complex *mexpu, 
			      int iexpd, const double complex * mexpd, 
			      double complex *local);

void LocalToLocal(const Box *ibox);

void LocalToTarget(const Box *ibox);

void ProcessList13(const Box *ibox);

void ProcessList4(const Box *ibox);

void DirectEvaluation(const Box *tbox, const Box *sbox);

void MakeUList(int nexpo, const double complex *expo, const int *list, int nlist, 
	       const int *xoff, const int *yoff, const double complex *xs,
	       const double complex *ys, double complex *mexpo);

void MakeDList(int nexpo, const double complex *expo, const int *list, int nlist, 
	       const int *xoff, const int *yoff, const double complex *xs, 
	       const double complex *ys, double complex *mexpo);

void rotz2y(const double complex *multipole, const double *rd, double complex *mrotate);

void roty2z(const double complex *multipole, const double *rd, double complex *mrotate);

void rotz2x(const double complex *multipole, const double *rd, double complex *mrotate);

#endif
