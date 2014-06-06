#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hpx/hpx.h"
#include "fmm-dag.h"
#include "fmm-types.h"
#include "fmm-action.h"

const int xoff[] = {0, 1, 0, 1, 0, 1, 0, 1};
const int yoff[] = {0, 0, 1, 1, 0, 0, 1, 1};
const int zoff[] = {0, 0, 0, 0, 1, 1, 1, 1};

int *sswap, *srecord, *tswap, *trecord, *buffer1, *buffer4; 

fmm_dag_t *fmm_dag;

hpx_addr_t list_done; 

static inline void update_list(int *list, int *nlist, int *xoff, int *yoff, 
			       int boxid, int ix, int iy) {
  list[*nlist] = boxid; 
  xoff[*nlist] = ix; 
  yoff[*nlist] = iy; 
  (*nlist)++;
}


fmm_dag_t *construct_dag(const double *sources, int nsources, 
			const double *targets, int ntargets, int s) {
  fmm_dag = calloc(1, sizeof(fmm_dag_t)); 

  fmm_dag->nslev = 0; 
  fmm_dag->nsboxes = 0; 
  fmm_dag->ntlev = 0; 
  fmm_dag->ntboxes = 0; 

  double xmin = sources[0]; 
  double xmax = sources[0]; 
  double ymin = sources[1];
  double ymax = sources[1];
  double zmin = sources[2]; 
  double zmax = sources[2]; 

  for (int i = 1; i < nsources; i++) {
    int j = 3*i;
    xmin = fmin(xmin, sources[j]);
    xmax = fmax(xmax, sources[j]);
    ymin = fmin(ymin, sources[j+1]);
    ymax = fmax(ymax, sources[j+1]);
    zmin = fmin(zmin, sources[j+2]);
    zmax = fmax(zmax, sources[j+2]);
  }

  for (int i = 0; i < ntargets; i++) {
    int j = 3*i;
    xmin = fmin(xmin, targets[j]);
    xmax = fmax(xmax, targets[j]);
    ymin = fmin(ymin, targets[j+1]);
    ymax = fmax(ymax, targets[j+1]);
    zmin = fmin(zmin, targets[j+2]);
    zmax = fmax(zmax, targets[j+2]);
  }

  fmm_dag->size = fmax(fmax(xmax - xmin, ymax - ymin), zmax - zmin); 
  fmm_dag->corner[0] = (xmax + xmin - fmm_dag->size)*0.5; 
  fmm_dag->corner[1] = (ymax + ymin - fmm_dag->size)*0.5; 
  fmm_dag->corner[2] = (zmax + zmin - fmm_dag->size)*0.5; 

  fmm_dag->mapsrc = calloc(nsources, sizeof(int)); 
  fmm_dag->maptar = calloc(ntargets, sizeof(int)); 
  
  sswap = calloc(nsources, sizeof(int)); 
  tswap = calloc(ntargets, sizeof(int)); 
  srecord = calloc(nsources, sizeof(int)); 
  trecord = calloc(ntargets, sizeof(int)); 

  for (int i = 0; i < nsources; i++) 
    fmm_dag->mapsrc[i] = i;

  for (int i = 0; i < ntargets; i++) 
    fmm_dag->maptar[i] = i; 

  fmm_dag->sboxes[0] = calloc(1, sizeof(fmm_box_t)); 
  fmm_dag->sboxes[0][0] = (fmm_box_t) {0, ++fmm_dag->nsboxes, 0, 
				       {0, 0, 0, 0, 0, 0, 0, 0}, 
				       0, 0, 0, 0, nsources, 0, 
				       0, 0, 0, 0}; 
  fmm_dag->tboxes[0] = calloc(1, sizeof(fmm_box_t)); 
  fmm_dag->tboxes[0][0] = (fmm_box_t) {0, ++fmm_dag->ntboxes, 0, 
				       {0, 0, 0, 0, 0, 0, 0, 0}, 
				       0, 0, 0, 0, ntargets, 0, 
				       0, 0, 0, 0}; 
  fmm_dag->tboxes[0][0].list5 = calloc(2, sizeof(int)); 
  fmm_dag->tboxes[0][0].list5[0] = 1; 
  fmm_dag->tboxes[0][0].list5[1] = 1; 

  int ns = 1, nt = 1; 
  double h = fmm_dag->size; 

  for (int lev = 0; lev <= MAXLEV; lev++) {
    int sbox0 = fmm_dag->sboxes[lev][0].boxid; 
    int mp = 0; 

    for (int ibox = 0; ibox < nt; ibox++) {
      if (fmm_dag->tboxes[lev][ibox].npts > s && 
	  fmm_dag->tboxes[lev][ibox].list5) {
	int *list5 = &fmm_dag->tboxes[lev][ibox].list5[1]; 
	int nlist5 = fmm_dag->tboxes[lev][ibox].list5[0]; 
	for (int j = 0; j < nlist5; j++) {
	  int index = list5[j] - sbox0; 
	  if (fmm_dag->sboxes[lev][index].npts > s) {
	    fmm_dag->tboxes[lev][ibox].nchild = 1; 
	    fmm_dag->sboxes[lev][index].nchild = 1; 
	    mp = 1;
	  }
	}
      }
    }

    h /= 2; 

    if (mp) {
      for (int ibox = 0; ibox < ns; ibox++) {
	if (fmm_dag->sboxes[lev][ibox].nchild) 
	  partition_box(lev, ibox, sources, h, 'S'); 
      }

      int nns = 0; 
      for (int ibox = 0; ibox < ns; ibox++) 
	nns += fmm_dag->sboxes[lev][ibox].nchild; 

      fmm_dag->sboxes[lev + 1] = calloc(nns, sizeof(fmm_box_t)); 

      int iter = 0; 
      for (int ibox = 0; ibox < ns; ibox++) {
	fmm_box_t *pbox = &fmm_dag->sboxes[lev][ibox]; 
	if (pbox->nchild) {
	  int offset = 0; 
	  for (int i = 0; i < 8; i++) {
	    if (pbox->child[i]) {
	      fmm_box_t *cbox = &fmm_dag->sboxes[lev + 1][iter]; 
	      cbox->level = lev + 1; 
	      cbox->boxid = ++fmm_dag->nsboxes; 
	      cbox->parent = pbox->boxid; 
	      cbox->idx = 2*pbox->idx + xoff[i]; 
	      cbox->idy = 2*pbox->idy + yoff[i]; 
	      cbox->idz = 2*pbox->idz + zoff[i]; 
	      cbox->npts = pbox->child[i]; 
	      cbox->addr = pbox->addr + offset; 
	      offset += cbox->npts; 
	      pbox->child[i] = cbox->boxid; 
	      iter++; 
	    }
	  }
	}
      }

      ns = nns;
    } else {
      fmm_dag->nslev = lev; 
    } 

    if (mp) {
      for (int ibox = 0; ibox < nt; ibox++) {
	if (fmm_dag->tboxes[lev][ibox].nchild) 
	  partition_box(lev, ibox, targets, h, 'T'); 
      }

      int nnt = 0; 
      for (int ibox = 0; ibox < nt; ibox++) 
	nnt += fmm_dag->tboxes[lev][ibox].nchild; 

      fmm_dag->tboxes[lev + 1] = calloc(nnt, sizeof(fmm_box_t)); 

      int iter = 0; 
      for (int ibox = 0; ibox < nt; ibox++) {
	fmm_box_t *pbox = &fmm_dag->tboxes[lev][ibox]; 
	if (pbox->nchild) {
	  int offset = 0; 
	  for (int i = 0; i < 8; i++) {
	    if (pbox->child[i]) {
	      fmm_box_t *cbox = &fmm_dag->tboxes[lev + 1][iter]; 
	      cbox->level = lev + 1;
	      cbox->boxid = ++fmm_dag->ntboxes; 
	      cbox->parent = pbox->boxid; 
	      cbox->idx = 2*pbox->idx + xoff[i]; 
	      cbox->idy = 2*pbox->idy + yoff[i]; 
	      cbox->idz = 2*pbox->idz + zoff[i]; 
	      cbox->npts = pbox->child[i]; 
	      cbox->addr = pbox->addr + offset; 
	      offset += cbox->npts; 
	      pbox->child[i] = cbox->boxid; 
	      iter++; 
	    }
	  }
	}
      }

      for (int ibox = 0; ibox < nnt; ibox++) {
	int nlist5 = 0, temp5[27] = {0}; 
	fmm_box_t *tbox = &fmm_dag->tboxes[lev + 1][ibox]; 
	int tidx = tbox->idx; 
	int tidy = tbox->idy;
	int tidz = tbox->idz; 
	int index = tbox->parent - fmm_dag->tboxes[lev][0].boxid; 
	int *plist5 = &fmm_dag->tboxes[lev][index].list5[1]; 
	int nplist5 = fmm_dag->tboxes[lev][index].list5[0]; 

	for (int j = 0; j < nplist5; j++) {
	  index = plist5[j] - sbox0; 
	  for (int k = 0; k < 8; k++) {
	    int child = fmm_dag->sboxes[lev][index].child[k]; 
	    if (child) {
	      int index1 = child - fmm_dag->sboxes[lev + 1][0].boxid; 
	      fmm_box_t *sbox = &fmm_dag->sboxes[lev + 1][index1]; 
	      int sidx = sbox->idx; 
	      int sidy = sbox->idy; 
	      int sidz = sbox->idz; 
	      int diffx = fabs(tidx - sidx) <= 1; 
	      int diffy = fabs(tidy - sidy) <= 1;
	      int diffz = fabs(tidz - sidz) <= 1; 
	      if (diffx*diffy*diffz) 
		temp5[nlist5++] = child;
	    }
	  }
	}

	tbox->list5 = calloc(1 + nlist5, sizeof(int)); 
	tbox->list5[0] = nlist5; 
	memcpy(&tbox->list5[1], temp5, sizeof(int)*nlist5); 
      }

      nt = nnt; 
    } else {
      fmm_dag->ntlev = lev; 
      break;
    }              
  }

  fmm_dag->sboxptrs = calloc(1 + fmm_dag->nsboxes, sizeof(fmm_box_t *)); 
  fmm_dag->tboxptrs = calloc(1 + fmm_dag->ntboxes, sizeof(fmm_box_t *)); 

  int begin, end; 
  for (int lev = 0; lev < fmm_dag->nslev; lev++) {
    begin = fmm_dag->sboxes[lev][0].boxid; 
    end   = fmm_dag->sboxes[lev + 1][0].boxid - 1; 
    for (int j = begin; j <= end; j++) 
      fmm_dag->sboxptrs[j] = &fmm_dag->sboxes[lev][j - begin]; 
  }

  begin = fmm_dag->sboxes[fmm_dag->nslev][0].boxid; 
  end   = fmm_dag->nsboxes; 
  for (int j = begin; j <= end; j++) 
    fmm_dag->sboxptrs[j] = &fmm_dag->sboxes[fmm_dag->nslev][j - begin]; 

  for (int lev = 0; lev < fmm_dag->ntlev; lev++) {
    begin = fmm_dag->tboxes[lev][0].boxid; 
    end   = fmm_dag->tboxes[lev + 1][0].boxid - 1; 
    for (int j = begin; j <= end; j++) 
      fmm_dag->tboxptrs[j] = &fmm_dag->tboxes[lev][j - begin]; 
  }

  begin = fmm_dag->tboxes[fmm_dag->ntlev][0].boxid; 
  end   = fmm_dag->ntboxes; 
  for (int j = begin; j <= end; j++) 
    fmm_dag->tboxptrs[j] = &fmm_dag->tboxes[fmm_dag->ntlev][j - begin]; 

  free(sswap); 
  free(srecord); 
  free(tswap); 
  free(trecord); 

  buffer1 = calloc(27*(1 + fmm_dag->ntboxes), sizeof(int)); 
  buffer4 = calloc(24*(1 + fmm_dag->ntboxes), sizeof(int)); 

  // construct lists 1, 3, 4 edges for each target box in parallel
  list_done = hpx_lco_future_array_new(1 + fmm_dag->ntboxes, 0, 
				       1 + fmm_dag->ntboxes); 
  // set up the root node 
  hpx_lco_set(hpx_lco_future_array_at(list_done, 1), NULL, 0, HPX_NULL); 

  for (int i = 0; i < 8; i++) {
    int child = fmm_dag->tboxes[0][0].child[i]; 
    if (child) 
      hpx_call(HPX_HERE, _fmm_build_list134, &child, sizeof(child), HPX_NULL); 
  }

  for (int i = 1; i <= fmm_dag->ntboxes; i++) 
    hpx_lco_wait(hpx_lco_future_array_at(list_done, i)); 
	       
  //hpx_lco_future_array_delete(list_done); 

  free(buffer1); 
  free(buffer4); 

  return fmm_dag;
}


void destruct_dag(fmm_dag_t *fmm_dag) {
  for (int lev = 0; lev <= fmm_dag->nslev; lev++) 
    free(fmm_dag->sboxes[lev]); 
  free(fmm_dag->mapsrc);
  free(fmm_dag->sboxptrs); 

  for (int i = 1; i <= fmm_dag->ntboxes; i++) {
    fmm_box_t *tbox = fmm_dag->tboxptrs[i]; 
    free(tbox->list1);
    free(tbox->list3);
    free(tbox->list4);
    free(tbox->list5);
  }

  for (int lev = 0; lev <= fmm_dag->ntlev; lev++) 
    free(fmm_dag->tboxes[lev]); 
  free(fmm_dag->maptar); 
  free(fmm_dag->tboxptrs); 
}


void partition_box(int level, int index, 
		   const double *points, double h, char tag) {
  fmm_box_t *ibox; 
  int npoints, begin, *imap, *swap, *record; 

  if (tag == 'S') {
    ibox    = &(fmm_dag->sboxes[level][index]); 
    npoints = ibox->npts; 
    begin   = ibox->addr; 
    imap    = &(fmm_dag->mapsrc[begin]); 
    swap    = &sswap[begin]; 
    record  = &srecord[begin]; 
  } else {
    ibox    = &(fmm_dag->tboxes[level][index]); 
    npoints = ibox->npts; 
    begin   = ibox->addr; 
    imap    = &(fmm_dag->maptar[begin]); 
    swap    = &tswap[begin]; 
    record  = &trecord[begin]; 
  }

  double xc = fmm_dag->corner[0] + (2*ibox->idx + 1)*h;
  double yc = fmm_dag->corner[1] + (2*ibox->idy + 1)*h;
  double zc = fmm_dag->corner[2] + (2*ibox->idz + 1)*h; 

  int addrs[8] = {0}, assigned[8] = {0}; 

  for (int i = 0; i < npoints; i++) {
    int j = 3*imap[i]; 
    int bin = 4*(points[j + 2] > zc) + 2*(points[j + 1] > yc) + 
      (points[j] > xc); 
    ibox->child[bin]++; 
    record[i] = bin;
  }

  addrs[1] = addrs[0] + ibox->child[0]; 
  addrs[2] = addrs[1] + ibox->child[1]; 
  addrs[3] = addrs[2] + ibox->child[2]; 
  addrs[4] = addrs[3] + ibox->child[3]; 
  addrs[5] = addrs[4] + ibox->child[4]; 
  addrs[6] = addrs[5] + ibox->child[5]; 
  addrs[7] = addrs[6] + ibox->child[6]; 

  for (int i = 0; i < npoints; i++) {
    int bin = record[i]; 
    int offset = addrs[bin] + assigned[bin]; 
    swap[offset] = imap[i]; 
    assigned[bin]++; 
  }

  for (int i = 0; i < npoints; i++) 
    imap[i] = swap[i]; 
  
  ibox->nchild = 0; 
 
  ibox->nchild = (ibox->child[0] > 0) + (ibox->child[1] > 0) + 
    (ibox->child[2] > 0) + (ibox->child[3] > 0) + 
    (ibox->child[4] > 0) + (ibox->child[5] > 0) + 
    (ibox->child[6] > 0) + (ibox->child[7] > 0);     
}


int _fmm_build_list134_action(void *args) {
  int boxid = *(int *)args; 
  fmm_box_t *tbox = fmm_dag->tboxptrs[boxid]; 
  int nlist1 = 0, nlist4 = 0; 
  int *temp1 = &buffer1[27*boxid]; 
  int *temp4 = &buffer4[24*boxid]; 
  fmm_box_t *parent = fmm_dag->tboxptrs[tbox->parent]; 
  int *plist1 = (parent->list1 ? &parent->list1[1] : 0); 
  int nplist1 = (plist1 ? parent->list1[0] : 0); 

  for (int iter = 0; iter < nplist1; iter++) {
    fmm_box_t *sbox = fmm_dag->sboxptrs[plist1[iter]]; 
    if (is_adjacent(sbox, tbox)) {
      temp1[nlist1++] = sbox->boxid; 
    } else {
      temp4[nlist4++] = sbox->boxid; 
    }
  }

  if (nlist4) {
    tbox->list4 = calloc(1 + nlist4, sizeof(int)); 
    tbox->list4[0] = nlist4; 
    memcpy(&tbox->list4[1], temp4, sizeof(int)*nlist4); 
  }

  if (tbox->nchild) {
    int *list5 = &tbox->list5[1]; 
    int nlist5 = tbox->list5[0]; 
    for (int j = 0; j < nlist5; j++) {
      fmm_box_t *sbox = fmm_dag->sboxptrs[list5[j]]; 
      if (!sbox->nchild) 
	temp1[nlist1++] = sbox->boxid; 
    }

    if (nlist1) {
      tbox->list1 = calloc(1 + nlist1, sizeof(int)); 
      tbox->list1[0] = nlist1; 
      memcpy(&tbox->list1[1], temp1, sizeof(int)*nlist1); 
    } 

    hpx_lco_set(hpx_lco_future_array_at(list_done, boxid), NULL, 0, HPX_NULL); 

    for (int j = 0; j < 8; j++) {
      int child = tbox->child[j]; 
      if (child) 
	hpx_call(HPX_HERE, _fmm_build_list134, &child, 
		 sizeof(child), HPX_NULL); 
    }
  } else {
    if (tbox->list5) 
      build_list13(boxid, temp1, nlist1);

    hpx_lco_set(hpx_lco_future_array_at(list_done, boxid), NULL, 0, HPX_NULL); 
  }
  
  return HPX_SUCCESS; 
}


void build_list13(int boxid, const int *coarse_list, int ncoarse_list) {
  fmm_box_t *tbox = fmm_dag->tboxptrs[boxid]; 
  int *list5 = &tbox->list5[1]; 
  int nlist5 = tbox->list5[0]; 
  int level = tbox->level; 
  int M = fmm_dag->nslev - level, M2 = 1 << M, M4 = M2*M2; 
  int nlist1 = 0; 
  int nlist3 = 0; 
  int bufsz1 = nlist5*M4, bufsz3 = 8*M4 + 72*M2 + 56*M - 80; 
  int *temp1 = calloc(bufsz1, sizeof(int)); 
  int *temp3 = calloc(bufsz3, sizeof(int)); 

  for (int j = 0; j < nlist5; j++) {
    fmm_box_t *sbox = fmm_dag->sboxptrs[list5[j]]; 
    build_list13_from_box(tbox, sbox, temp1, &nlist1, temp3, &nlist3);
  }

  if (nlist1 + ncoarse_list) {
    tbox->list1 = calloc(1 + nlist1 + ncoarse_list, sizeof(int)); 
    tbox->list1[0] = nlist1 + ncoarse_list; 
    memcpy(&tbox->list1[1], coarse_list, sizeof(int)*ncoarse_list); 
    memcpy(&tbox->list1[1 + ncoarse_list], temp1, sizeof(int)*nlist1); 
  }
  
  if (nlist3) {
    tbox->list3 = calloc(1 + nlist3, sizeof(int)); 
    tbox->list3[0] = nlist3; 
    memcpy(&tbox->list3[1], temp3, sizeof(int)*nlist3); 
  }

  free(temp1); 
  free(temp3); 
}


void build_list13_from_box(fmm_box_t *tbox, fmm_box_t *sbox, int *list1, 
			   int *nlist1, int *list3, int *nlist3) {
  if (is_adjacent(tbox, sbox)) {
    if (sbox->nchild) {
      for (int j = 0; j < 8; j++) {
	int child = sbox->child[j]; 
	if (child) 
	  build_list13_from_box(tbox, fmm_dag->sboxptrs[child], 
	  			list1, nlist1, list3, nlist3); 
      }
    } else {
      list1[*nlist1] = sbox->boxid;
      (*nlist1)++; 
    } 
  } else {
    list3[*nlist3] = sbox->boxid; 
    (*nlist3)++;
  }
}


int is_adjacent(const fmm_box_t *box1, const fmm_box_t *box2) {
  int dim = 1 << (box2->level - box1->level); 

  return ((box2->idx >= dim*box1->idx - 1) && 
	  (box2->idx <= dim*box1->idx + dim) && 
	  (box2->idy >= dim*box1->idy - 1) && 
	  (box2->idy <= dim*box1->idy + dim) &&
	  (box2->idz >= dim*box1->idz - 1) && 
	  (box2->idz <= dim*box1->idz + dim)); 
}


void build_merged_list2(const fmm_dag_t *fmm_dag, 
			disaggr_nonleaf_arg_t *disaggr_nonleaf_arg) {
  int boxid = disaggr_nonleaf_arg->boxid; 

  int *uall = disaggr_nonleaf_arg->uall;
  int *nuall = &(disaggr_nonleaf_arg->nuall); 
  int *xuall = disaggr_nonleaf_arg->xuall;
  int *yuall = disaggr_nonleaf_arg->yuall;
  int *u1234 = disaggr_nonleaf_arg->u1234;
  int *nu1234 = &(disaggr_nonleaf_arg->nu1234); 
  int *x1234 = disaggr_nonleaf_arg->x1234;
  int *y1234 = disaggr_nonleaf_arg->y1234; 

  int *dall = disaggr_nonleaf_arg->dall;
  int *ndall = &(disaggr_nonleaf_arg->ndall); 
  int *xdall = disaggr_nonleaf_arg->xdall; 
  int *ydall = disaggr_nonleaf_arg->ydall; 
  int *d5678 = disaggr_nonleaf_arg->d5678; 
  int *nd5678 = &(disaggr_nonleaf_arg->nd5678); 
  int *x5678 = disaggr_nonleaf_arg->x5678; 
  int *y5678 = disaggr_nonleaf_arg->y5678;

  int *nall = disaggr_nonleaf_arg->nall;
  int *nnall = &(disaggr_nonleaf_arg->nnall); 
  int *xnall = disaggr_nonleaf_arg->xnall; 
  int *ynall = disaggr_nonleaf_arg->ynall;
  int *n1256 = disaggr_nonleaf_arg->n1256;
  int *nn1256 = &(disaggr_nonleaf_arg->nn1256); 
  int *x1256 = disaggr_nonleaf_arg->x1256;
  int *y1256 = disaggr_nonleaf_arg->y1256;
  int *n12 = disaggr_nonleaf_arg->n12;
  int *nn12 = &(disaggr_nonleaf_arg->nn12);
  int *x12 = disaggr_nonleaf_arg->x12;
  int *y12 = disaggr_nonleaf_arg->y12;
  int *n56 = disaggr_nonleaf_arg->n56;
  int *nn56 = &(disaggr_nonleaf_arg->nn56);
  int *x56 = disaggr_nonleaf_arg->x56;
  int *y56 = disaggr_nonleaf_arg->y56;

  int *sall = disaggr_nonleaf_arg->sall; 
  int *nsall = &(disaggr_nonleaf_arg->nsall); 
  int *xsall = disaggr_nonleaf_arg->xsall;
  int *ysall = disaggr_nonleaf_arg->ysall; 
  int *s3478 = disaggr_nonleaf_arg->s3478;
  int *ns3478 = &(disaggr_nonleaf_arg->ns3478); 
  int *x3478 = disaggr_nonleaf_arg->x3478;
  int *y3478 = disaggr_nonleaf_arg->y3478;
  int *s34 = disaggr_nonleaf_arg->s34;
  int *ns34 = &(disaggr_nonleaf_arg->ns34); 
  int *x34 = disaggr_nonleaf_arg->x34;
  int *y34 = disaggr_nonleaf_arg->y34;
  int *s78 = disaggr_nonleaf_arg->s78;
  int *ns78 = &(disaggr_nonleaf_arg->ns78); 
  int *x78 = disaggr_nonleaf_arg->x78;
  int *y78 = disaggr_nonleaf_arg->y78;


  int *eall = disaggr_nonleaf_arg->eall; 
  int *neall = &(disaggr_nonleaf_arg->neall); 
  int *xeall = disaggr_nonleaf_arg->xeall;
  int *yeall = disaggr_nonleaf_arg->yeall; 
  int *e1357 = disaggr_nonleaf_arg->e1357; 
  int *ne1357 = &(disaggr_nonleaf_arg->ne1357); 
  int *x1357 = disaggr_nonleaf_arg->x1357;
  int *y1357 = disaggr_nonleaf_arg->y1357; 
  int *e13 = disaggr_nonleaf_arg->e13; 
  int *ne13 = &(disaggr_nonleaf_arg->ne13); 
  int *x13 = disaggr_nonleaf_arg->x13;
  int *y13 = disaggr_nonleaf_arg->y13; 
  int *e57 = disaggr_nonleaf_arg->e57; 
  int *ne57 = &(disaggr_nonleaf_arg->ne57); 
  int *x57 = disaggr_nonleaf_arg->x57;
  int *y57 = disaggr_nonleaf_arg->y57; 
  int *e1 = disaggr_nonleaf_arg->e1; 
  int *ne1 = &(disaggr_nonleaf_arg->ne1); 
  int *x1 = disaggr_nonleaf_arg->x1;
  int *y1 = disaggr_nonleaf_arg->y1; 
  int *e3 = disaggr_nonleaf_arg->e3; 
  int *ne3 = &(disaggr_nonleaf_arg->ne3); 
  int *x3 = disaggr_nonleaf_arg->x3;
  int *y3 = disaggr_nonleaf_arg->y3; 
  int *e5 = disaggr_nonleaf_arg->e5; 
  int *ne5 = &(disaggr_nonleaf_arg->ne5); 
  int *x5 = disaggr_nonleaf_arg->x5;
  int *y5 = disaggr_nonleaf_arg->y5; 
  int *e7 = disaggr_nonleaf_arg->e7; 
  int *ne7 = &(disaggr_nonleaf_arg->ne7); 
  int *x7 = disaggr_nonleaf_arg->x7;
  int *y7 = disaggr_nonleaf_arg->y7; 

  int *wall = disaggr_nonleaf_arg->wall; 
  int *nwall = &(disaggr_nonleaf_arg->nwall); 
  int *xwall = disaggr_nonleaf_arg->xwall;
  int *ywall = disaggr_nonleaf_arg->ywall; 
  int *w2468 = disaggr_nonleaf_arg->w2468; 
  int *nw2468 = &(disaggr_nonleaf_arg->nw2468); 
  int *x2468 = disaggr_nonleaf_arg->x2468;
  int *y2468 = disaggr_nonleaf_arg->y2468; 
  int *w24 = disaggr_nonleaf_arg->w24; 
  int *nw24 = &(disaggr_nonleaf_arg->nw24); 
  int *x24 = disaggr_nonleaf_arg->x24;
  int *y24 = disaggr_nonleaf_arg->y24; 
  int *w68 = disaggr_nonleaf_arg->w68; 
  int *nw68 = &(disaggr_nonleaf_arg->nw68); 
  int *x68 = disaggr_nonleaf_arg->x68;
  int *y68 = disaggr_nonleaf_arg->y68; 
  int *w2 = disaggr_nonleaf_arg->w2; 
  int *nw2 = &(disaggr_nonleaf_arg->nw2); 
  int *x2 = disaggr_nonleaf_arg->x2;
  int *y2 = disaggr_nonleaf_arg->y2; 
  int *w4 = disaggr_nonleaf_arg->w4; 
  int *nw4 = &(disaggr_nonleaf_arg->nw4); 
  int *x4 = disaggr_nonleaf_arg->x4;
  int *y4 = disaggr_nonleaf_arg->y4; 
  int *w6 = disaggr_nonleaf_arg->w6; 
  int *nw6 = &(disaggr_nonleaf_arg->nw6); 
  int *x6 = disaggr_nonleaf_arg->x6;
  int *y6 = disaggr_nonleaf_arg->y6; 
  int *w8 = disaggr_nonleaf_arg->w8; 
  int *nw8 = &(disaggr_nonleaf_arg->nw8); 
  int *x8 = disaggr_nonleaf_arg->x8;
  int *y8 = disaggr_nonleaf_arg->y8; 

  *nuall = *nu1234 = 0;
  *ndall = *nd5678 = 0;
  *nnall = *nn1256 = *nn12 = *nn56 = 0;
  *nsall = *ns3478 = *ns34 = *ns78 = 0;
  *neall = *ne1357 = *ne13 = *ne57 = *ne1 = *ne3 = *ne5 = *ne7 = 0;
  *nwall = *nw2468 = *nw24 = *nw68 = *nw2 = *nw4 = *nw6 = *nw8 = 0;

  const fmm_box_t *tbox = fmm_dag->tboxptrs[boxid]; 
  int tidx = tbox->idx; 
  int tidy = tbox->idy; 
  int tidz = tbox->idz; 
  int *list5 = &tbox->list5[1]; 
  int nlist5 = tbox->list5[0]; 

  for (int j = 0; j < nlist5; j++) {
    const fmm_box_t *sbox = fmm_dag->sboxptrs[list5[j]]; 
    int sidx = sbox->idx; 
    int sidy = sbox->idy; 
    int sidz = sbox->idz; 
    int offset = 9*(sidz - tidz) + 3*(sidy - tidy) + sidx - tidx + 13; 

    switch (offset) {
    case 0:
      // [-1][-1][-1], update dall, sall, wall, d5678, s34, w2 lists
      if (sbox->child[0]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[0], -2, -2);
      if (sbox->child[1]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[1], -1,-2);
      if (sbox->child[2]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[2], -2, -1);
      if (sbox->child[3]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[3], -1,-1);
      if (sbox->child[4]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[4], -1, -2);
      if (sbox->child[5]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[5], -1, -1);
      if (sbox->child[6]) 
	update_list(wall, nwall, xwall, ywall, sbox->child[6], 1, -1);
      if (sbox->child[7]) {
	update_list(d5678, nd5678, x5678, y5678, sbox->child[7], -1, -1);
	update_list(s34, ns34, x34, y34, sbox->child[7], -1, -1);
	update_list(w2, nw2, x2, y2, sbox->child[7], 1, -1);
      }
      break;
    case 1://[0][-1][-1], update dall, sall, d5678, s34 lists
      if (sbox->child[0]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[0], 0, -2);
      if (sbox->child[1]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[1], 1, -2);
      if (sbox->child[2]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[2], 0, -1);
      if (sbox->child[3]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[3], 1,-1);
      if (sbox->child[4]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[4], -1, 0);
      if (sbox->child[5]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[5], -1, 1);     
      if (sbox->child[6]) {
	update_list(d5678, nd5678, x5678, y5678, sbox->child[6], 0, -1);
	update_list(s34, ns34, x34, y34, sbox->child[6], -1,0);
      }
      if (sbox->child[7]) {
	update_list(d5678, nd5678, x5678, y5678, sbox->child[7], 1, -1);
	update_list(s34, ns34, x34, y34, sbox->child[7], -1, 1);
      }
      break;
    case 2: //[1][-1][-1], update dall, sall, eall, d5678, s34, e1 lists
      if (sbox->child[0]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[0], 2, -2);
      if (sbox->child[1]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[1], 3, -2);
      if (sbox->child[2]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[2], 2, -1);
      if (sbox->child[3]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[3], 3, -1);
      if (sbox->child[4]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[4], -1, 2);
      if (sbox->child[5]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[5], -1, 3);
      if (sbox->child[6]) {
	update_list(d5678, nd5678, x5678, y5678, sbox->child[6], 2, -1);
	update_list(s34, ns34, x34, y34, sbox->child[6], -1, 2);
	update_list(e1, ne1, x1, y1, sbox->child[6], 1, -1);
      }
      if (sbox->child[7]) 
	update_list(eall, neall, xeall, yeall, sbox->child[7], 1, -1);
      break;
    case 3: //[-1][0][-1], update dall, wall, d5678, w24 lists
      if (sbox->child[0]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[0], -2, 0);
      if (sbox->child[1]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[1], -1, 0);
      if (sbox->child[2]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[2], -2, 1);
      if (sbox->child[3]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[3], -1, 1);
      if (sbox->child[4]) 
	update_list(wall, nwall, xwall, ywall, sbox->child[4], 1, 0);
      if (sbox->child[5]) {
	update_list(d5678, nd5678, x5678, y5678, sbox->child[5], -1, 0);
	update_list(w24, nw24, x24, y24, sbox->child[5], 1, 0);
      }
      if (sbox->child[6]) 
	update_list(wall, nwall, xwall, ywall, sbox->child[6], 1, 1);
      if (sbox->child[7]) {
	update_list(d5678, nd5678, x5678, y5678, sbox->child[7], -1, 1);
	update_list(w24, nw24, x24, y24, sbox->child[7], 1, 1);
      }
      break;
    case 4://[0][0][-1], update dall and d5678 lists
      if (sbox->child[0]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[0], 0, 0);
      if (sbox->child[1]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[1], 1, 0);
      if (sbox->child[2]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[2], 0, 1);
      if (sbox->child[3]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[3], 1, 1);
      if (sbox->child[4]) 
	update_list(d5678, nd5678, x5678, y5678, sbox->child[4], 0, 0);
      if (sbox->child[5]) 
	update_list(d5678, nd5678, x5678, y5678, sbox->child[5], 1, 0);
      if (sbox->child[6]) 
	update_list(d5678, nd5678, x5678, y5678, sbox->child[6], 0, 1);
      if (sbox->child[7]) 
	update_list(d5678, nd5678, x5678, y5678, sbox->child[7], 1, 1);
      break;
    case 5: // [1][0][-1], update dall, d5678, eall, e13 lists
      if (sbox->child[0]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[0], 2, 0);
      if (sbox->child[1]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[1], 3, 0);
      if (sbox->child[2]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[2], 2, 1);
      if (sbox->child[3]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[3], 3, 1);
      if (sbox->child[4]) {
	update_list(d5678, nd5678, x5678, y5678, sbox->child[4], 2, 0);
	update_list(e13, ne13, x13, y13, sbox->child[4], 1, 0);
      }
      if (sbox->child[5]) 
	update_list(eall, neall, xeall, yeall, sbox->child[5], 1, 0);
      if (sbox->child[6]) {
	update_list(d5678, nd5678, x5678, y5678, sbox->child[6], 2, 1);
	update_list(e13, ne13, x13, y13, sbox->child[6], 1, 1);
      }
      if (sbox->child[7]) 
	update_list(eall, neall, xeall, yeall, sbox->child[7], 1, 1);
      break;
    case 6://[-1][1][-1], update dall, nall, wall, d5678, n12, w4 list3
      if (sbox->child[0]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[0], -2, 2);
      if (sbox->child[1]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[1], -1, 2);
      if (sbox->child[2]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[2], -2, 3);
      if (sbox->child[3]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[3], -1, 3);
      if (sbox->child[4]) 
	update_list(wall, nwall, xwall, ywall, sbox->child[4], 1, 2);
      if (sbox->child[5]) {
	update_list(d5678, nd5678, x5678, y5678, sbox->child[5], -1, 2);
	update_list(n12, nn12, x12, y12, sbox->child[5], -1, -1);
	update_list(w4, nw4, x4, y4, sbox->child[5], 1, 2);
      }
      if (sbox->child[6]) 
	update_list(nall, nnall, xnall, ynall, sbox->child[6], -1, -2);
      if (sbox->child[7]) 
	update_list(nall, nnall, xnall, ynall, sbox->child[7], -1, -1);
      break;
    case 7: //[0][1][-1], update dallm d5678, nall, n12 lists 
      if (sbox->child[0]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[0], 0, 2);
      if (sbox->child[1]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[1], 1, 2);
      if (sbox->child[2]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[2], 0, 3);
      if (sbox->child[3]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[3], 1, 3);
      if (sbox->child[4]) {
	update_list(d5678, nd5678, x5678, y5678, sbox->child[4], 0, 2);
	update_list(n12, nn12, x12, y12, sbox->child[4], -1, 0);
      }
      if (sbox->child[5]) {
	update_list(d5678, nd5678,x5678, y5678, sbox->child[5], 1, 2);
	update_list(n12, nn12, x12, y12, sbox->child[5], -1, 1);
      }
      if (sbox->child[6]) 
	update_list(nall, nnall, xnall, ynall, sbox->child[6], -1, 0);
      if (sbox->child[7])
	update_list(nall, nnall, xnall, ynall, sbox->child[7], -1, 1);
      break;
    case 8: //[1][1][-1], update dall, d5678, nall, eall, n12, e3 lists
      if (sbox->child[0]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[0], 2,2);
      if (sbox->child[1]) 
	update_list(dall, ndall, xdall, ydall, sbox->child[1], 3, 2);
      if (sbox->child[2])
	update_list(dall, ndall, xdall, ydall, sbox->child[2], 2, 3);
      if (sbox->child[3])
	update_list(dall, ndall, xdall, ydall, sbox->child[3], 3,3);
      if (sbox->child[4]) {
	update_list(d5678, nd5678, x5678, y5678, sbox->child[4], 2, 2);
	update_list(n12, nn12, x12, y12, sbox->child[4], -1, 2);
	update_list(e3, ne3, x3, y3, sbox->child[4], 1, 2);
      }
      if (sbox->child[5]) 
	update_list(eall, neall, xeall, yeall, sbox->child[5], 1, 2);
      if (sbox->child[6]) 
	update_list(nall, nnall, xnall, ynall, sbox->child[6], -1, 2);
      if (sbox->child[7]) 
	update_list(nall, nnall, xnall, ynall, sbox->child[7], -1, 3);
      break;
    case 9: // [-1][-1][0], update sall, wall, s3478, w2, w6 lists
      if (sbox->child[0]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[0], 0, -2);
      if (sbox->child[1])
	update_list(sall, nsall, xsall, ysall, sbox->child[1], 0, -1);
      if (sbox->child[2])
	update_list(wall, nwall, xwall, ywall, sbox->child[2], 0, -1);
      if (sbox->child[3]) {
	update_list(s3478, ns3478, x3478, y3478, sbox->child[3], 0, -1);
	update_list(w2, nw2, x2, y2, sbox->child[3], 0, -1);
	update_list(w6, nw6, x6, y6, sbox->child[3], 0, -1);
      }
      if (sbox->child[4]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[4], 1, -2);
      if (sbox->child[5])
	update_list(sall, nsall, xsall, ysall, sbox->child[5], 1, -1);
      if (sbox->child[6]) 
	update_list(wall, nwall, xwall, ywall, sbox->child[6], -1, -1);
      if (sbox->child[7]) {
	update_list(s3478, ns3478, x3478, y3478, sbox->child[7], 1, -1);
	update_list(w2, nw2, x2, y2, sbox->child[7], -1, -1);
	update_list(w6, nw6, x6, y6, sbox->child[7], -1, -1);
      }
      break;
    case 10://[0][-1][0], update sall, s3478 lists
      if (sbox->child[0]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[0], 0, 0);
      if (sbox->child[1])
	update_list(sall, nsall, xsall, ysall, sbox->child[1], 0, 1);
      if (sbox->child[2])
	update_list(s3478, ns3478, x3478, y3478, sbox->child[2], 0, 0);
      if (sbox->child[3])
	update_list(s3478, ns3478, x3478, y3478, sbox->child[3], 0, 1);
      if (sbox->child[4]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[4], 1, 0);
      if (sbox->child[5])
	update_list(sall, nsall, xsall, ysall, sbox->child[5], 1, 1);
      if (sbox->child[6])
	update_list(s3478, ns3478, x3478, y3478, sbox->child[6], 1, 0);
      if (sbox->child[7])
	update_list(s3478, ns3478, x3478, y3478, sbox->child[7], 1, 1);
      break;
    case 11: //[1][-1][0], update eall, sall, s3478, e1, e5 lists
      if (sbox->child[0])
	update_list(sall, nsall, xsall, ysall, sbox->child[0], 0, 2);
      if (sbox->child[1])
	update_list(sall, nsall, xsall, ysall, sbox->child[1], 0, 3);
      if (sbox->child[2]) {
	update_list(s3478, ns3478, x3478, y3478, sbox->child[2], 0, 2);
	update_list(e1, ne1, x1, y1, sbox->child[2], 0, -1);
	update_list(e5, ne5, x5, y5, sbox->child[2], 0, -1);
      }
      if (sbox->child[3]) 
	update_list(eall, neall, xeall, yeall, sbox->child[3], 0, -1);
      if (sbox->child[4])
	update_list(sall, nsall, xsall, ysall, sbox->child[4], 1, 2);
      if (sbox->child[5]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[5], 1, 3);
      if (sbox->child[6]) {
	update_list(s3478, ns3478, x3478, y3478, sbox->child[6], 1, 2);
	update_list(e1, ne1, x1, y1, sbox->child[6], -1, -1);
	update_list(e5, ne5, x5, y5, sbox->child[6], -1, -1);
      }
      if (sbox->child[7]) 
	update_list(eall, neall, xeall, yeall, sbox->child[7], -1, -1);
      break;
    case 12: // [-1][0][0], update wall, w2468 lists
      if (sbox->child[0])
	update_list(wall, nwall, xwall, ywall, sbox->child[0], 0, 0);
      if (sbox->child[1])
	update_list(w2468, nw2468, x2468, y2468, sbox->child[1], 0, 0);
      if (sbox->child[2])
	update_list(wall, nwall, xwall, ywall, sbox->child[2], 0, 1);
      if (sbox->child[3])
	update_list(w2468, nw2468, x2468, y2468, sbox->child[3], 0, 1);
      if (sbox->child[4])
	update_list(wall, nwall, xwall, ywall, sbox->child[4], -1, 0);
      if (sbox->child[5])
	update_list(w2468, nw2468, x2468, y2468, sbox->child[5], -1, 0);
      if (sbox->child[6])
	update_list(wall, nwall, xwall, ywall, sbox->child[6], -1, 1);
      if (sbox->child[7])
	update_list(w2468, nw2468, x2468, y2468, sbox->child[7], -1, 1);
      break;
    case 13: //[0][0][0], nothing here
      break;
    case 14: //[1][0][0], update eall, e1357 lists
      if (sbox->child[0])
	update_list(e1357, ne1357, x1357, y1357, sbox->child[0], 0, 0);
      if (sbox->child[1])
	update_list(eall, neall, xeall, yeall, sbox->child[1], 0, 0);
      if (sbox->child[2])
	update_list(e1357, ne1357, x1357, y1357, sbox->child[2], 0, 1);
      if (sbox->child[3])
	update_list(eall, neall, xeall, yeall, sbox->child[3], 0, 1);
      if (sbox->child[4]) 
	update_list(e1357, ne1357, x1357, y1357, sbox->child[4], -1, 0);
      if (sbox->child[5])
	update_list(eall, neall, xeall, yeall, sbox->child[5], -1, 0);
      if (sbox->child[6])
	update_list(e1357, ne1357, x1357, y1357, sbox->child[6], -1, 1);
      if (sbox->child[7])
	update_list(eall, neall, xeall, yeall, sbox->child[7], -1, 1);
      break;
    case 15://[-1][1][0], update wall, nall, n1256, w4, w8 lists
      if (sbox->child[0])
	update_list(wall, nwall, xwall, ywall, sbox->child[0], 0, 2);
      if (sbox->child[1]) {
	update_list(n1256, nn1256, x1256, y1256, sbox->child[1], 0, -1);
	update_list(w4, nw4, x4, y4, sbox->child[1], 0, 2);
	update_list(w8, nw8, x8, y8, sbox->child[1], 0, 2);
      }
      if (sbox->child[2])
	update_list(nall, nnall, xnall, ynall, sbox->child[2], 0, -2);
      if (sbox->child[3])
	update_list(nall, nnall, xnall, ynall, sbox->child[3], 0, -1);
      if (sbox->child[4])
	update_list(wall, nwall, xwall, ywall, sbox->child[4], -1, 2);
      if (sbox->child[5]) {
	update_list(n1256, nn1256, x1256, y1256, sbox->child[5], 1, -1);
	update_list(w4, nw4, x4, y4, sbox->child[5], -1, 2);
	update_list(w8, nw8, x8, y8, sbox->child[5], -1, 2);
      }
      if (sbox->child[6]) 
	update_list(nall, nnall, xnall, ynall, sbox->child[6], 1, -2);
      if (sbox->child[7])
	update_list(nall, nnall, xnall, ynall, sbox->child[7], 1, -1);
      break;
    case 16: //[0][1][0], update nall, n1256 lists
      if (sbox->child[0])
	update_list(n1256, nn1256, x1256, y1256, sbox->child[0], 0, 0);
      if (sbox->child[1])
	update_list(n1256, nn1256, x1256, y1256, sbox->child[1], 0, 1);
      if (sbox->child[2])
	update_list(nall, nnall, xnall, ynall, sbox->child[2], 0, 0);
      if (sbox->child[3])
	update_list(nall, nnall, xnall, ynall, sbox->child[3], 0, 1);
      if (sbox->child[4])
	update_list(n1256, nn1256, x1256, y1256, sbox->child[4], 1, 0);
      if (sbox->child[5])
	update_list(n1256, nn1256, x1256, y1256, sbox->child[5], 1, 1);
      if (sbox->child[6])
	update_list(nall, nnall, xnall, ynall, sbox->child[6], 1, 0);
      if (sbox->child[7])
	update_list(nall, nnall, xnall, ynall, sbox->child[7], 1, 1);
      break;
    case 17: //[1][1][0], update nall, n1256, eall, e3, e7 lists
      if (sbox->child[0]) {
	update_list(n1256, nn1256, x1256, y1256, sbox->child[0], 0, 2);
	update_list(e3, ne3, x3, y3, sbox->child[0], 0, 2);
	update_list(e7, ne7, x7, y7, sbox->child[0], 0, 2);
      }
      if (sbox->child[1]) 
	update_list(eall, neall, xeall, yeall, sbox->child[1], 0, 2);
      if (sbox->child[2])
	update_list(nall, nnall, xnall, ynall, sbox->child[2], 0, 2);
      if (sbox->child[3])
	update_list(nall, nnall, xnall, ynall, sbox->child[3], 0, 3);
      if (sbox->child[4]) {
	update_list(n1256, nn1256, x1256, y1256, sbox->child[4], 1, 2);
	update_list(e3, ne3, x3, y3, sbox->child[4], -1, 2);
	update_list(e7, ne7, x7, y7, sbox->child[4], -1, 2);
      }
      if (sbox->child[5])
	update_list(eall, neall, xeall, yeall, sbox->child[5], -1, 2);
      if (sbox->child[6])
	update_list(nall, nnall, xnall, ynall, sbox->child[6], 1, 2);
      if (sbox->child[7])
	update_list(nall, nnall, xnall, ynall, sbox->child[7], 1, 3);
      break;
    case 18: //[-1][-1][1], update sall, wall, u1234, s78, w6, uall lists
      if (sbox->child[0]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[0], 2, -2);
      if (sbox->child[1]) 
	update_list(sall, nsall, xsall, ysall, sbox->child[1], 2, -1);
      if (sbox->child[2]) 
	update_list(wall, nwall, xwall, ywall, sbox->child[2], -2, -1);
      if (sbox->child[3]) {
	update_list(u1234, nu1234, x1234, y1234, sbox->child[3], -1,-1);
	update_list(s78, ns78, x78, y78, sbox->child[3], 2, -1);
	update_list(w6, nw6, x6, y6, sbox->child[3], -2, -1);
      }
      if (sbox->child[4]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[4], -2, -2);
      if (sbox->child[5])
	update_list(uall, nuall, xuall, yuall, sbox->child[5], -1, -2);
      if (sbox->child[6])
	update_list(uall, nuall, xuall, yuall, sbox->child[6], -2, -1);
      if (sbox->child[7])
	update_list(uall, nuall, xuall, yuall, sbox->child[7], -1, -1);
      break;
    case 19: //[0][-1][1], update sall, u1234, s78, uall lists
      if (sbox->child[0])
	update_list(sall, nsall, xsall, ysall, sbox->child[0], 2, 0);
      if (sbox->child[1])
	update_list(sall, nsall, xsall, ysall, sbox->child[1], 2, 1);
      if (sbox->child[2]) {
	update_list(u1234, nu1234, x1234, y1234, sbox->child[2], 0, -1);
	update_list(s78, ns78, x78, y78, sbox->child[2], 2, 0);
      }
      if (sbox->child[3]) {
	update_list(u1234, nu1234, x1234, y1234, sbox->child[3], 1, -1);
	update_list(s78, ns78, x78, y78, sbox->child[3], 2, 1);
      }
      if (sbox->child[4]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[4], 0, -2);
      if (sbox->child[5])
	update_list(uall, nuall, xuall, yuall, sbox->child[5], 1, -2);
      if (sbox->child[6])
	update_list(uall, nuall, xuall, yuall, sbox->child[6], 0, -1);
      if (sbox->child[7])
	update_list(uall, nuall, xuall, yuall, sbox->child[7], 1, -1);
      break;
    case 20: // [1][-1][1], update sall, eall, u1234, s78, e5, uall lists
      if (sbox->child[0])
	update_list(sall, nsall, xsall, ysall, sbox->child[0], 2, 2);
      if (sbox->child[1])
	update_list(sall, nsall, xsall, ysall, sbox->child[1], 2, 3);
      if (sbox->child[2]) {
	update_list(u1234, nu1234, x1234, y1234, sbox->child[2], 2, -1);
	update_list(s78, ns78, x78, y78, sbox->child[2], 2, 2);
	update_list(e5, ne5, x5, y5, sbox->child[2], -2, -1);
      }
      if (sbox->child[3])
	update_list(eall, neall, xeall, yeall, sbox->child[3], -2, -1);
      if (sbox->child[4])
	update_list(uall, nuall, xuall, yuall, sbox->child[4], 2, -2);
      if (sbox->child[5])
	update_list(uall, nuall, xuall, yuall, sbox->child[5], 3, -2);
      if (sbox->child[6])
	update_list(uall, nuall, xuall, yuall, sbox->child[6], 2, -1);
      if (sbox->child[7])
	update_list(uall, nuall, xuall, yuall, sbox->child[7], 3, -1);
      break;
    case 21: // [-1][0][1], update wall, u1234, w68, uall lists
      if (sbox->child[0])
	update_list(wall, nwall, xwall, ywall, sbox->child[0], -2, 0);
      if (sbox->child[1]) {
	update_list(u1234, nu1234, x1234, y1234, sbox->child[1], -1, 0);
	update_list(w68, nw68, x68, y68, sbox->child[1], -2, 0);
      }
      if (sbox->child[2]) 
	update_list(wall, nwall, xwall, ywall, sbox->child[2], -2, 1);
      if (sbox->child[3]) {
	update_list(u1234, nu1234, x1234, y1234, sbox->child[3], -1, 1);
	update_list(w68, nw68, x68, y68, sbox->child[3], -2, 1);
      }
      if (sbox->child[4]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[4], -2, 0);
      if (sbox->child[5])
	update_list(uall, nuall, xuall, yuall, sbox->child[5], -1, 0);
      if (sbox->child[6])
	update_list(uall, nuall, xuall, yuall, sbox->child[6], -2, 1);
      if (sbox->child[7])
	update_list(uall, nuall, xuall, yuall, sbox->child[7], -1, 1);
      break;
    case 22: //[0][0][1], update u1234, uall lists
      if (sbox->child[0]) 
	update_list(u1234, nu1234, x1234, y1234, sbox->child[0], 0, 0);
      if (sbox->child[1])
	update_list(u1234, nu1234, x1234, y1234, sbox->child[1], 1, 0);
      if (sbox->child[2]) 
	update_list(u1234, nu1234, x1234, y1234, sbox->child[2], 0, 1);
      if (sbox->child[3])
	update_list(u1234, nu1234, x1234, y1234, sbox->child[3], 1, 1);
      if (sbox->child[4]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[4], 0, 0);
      if (sbox->child[5])
	update_list(uall, nuall, xuall, yuall, sbox->child[5], 1, 0);
      if (sbox->child[6]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[6], 0, 1);
      if (sbox->child[7])
	update_list(uall, nuall, xuall, yuall, sbox->child[7], 1, 1);
      break;
    case 23: // [1][0][1], update u1234, e57, eall, uall lists
      if (sbox->child[0]) {
	update_list(u1234, nu1234, x1234, y1234, sbox->child[0], 2, 0);
	update_list(e57, ne57, x57, y57, sbox->child[0], -2, 0);
      }
      if (sbox->child[1])
	update_list(eall, neall, xeall, yeall, sbox->child[1], -2, 0);
      if (sbox->child[2]) {
	update_list(u1234, nu1234, x1234, y1234, sbox->child[2], 2, 1);
	update_list(e57, ne57, x57, y57, sbox->child[2], -2, 1);
      }
      if (sbox->child[3])
	update_list(eall, neall, xeall, yeall, sbox->child[3], -2, 1);
      if (sbox->child[4]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[4], 2, 0);
      if (sbox->child[5])
	update_list(uall, nuall, xuall, yuall, sbox->child[5], 3, 0);
      if (sbox->child[6]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[6], 2, 1);
      if (sbox->child[7])
	update_list(uall, nuall, xuall, yuall, sbox->child[7], 3, 1);
      break;
    case 24: // [-1][1][1], update nall, wall, u1234, n56, w8, uall lists
      if (sbox->child[0]) 
	update_list(wall, nwall, xwall, ywall, sbox->child[0], -2, 2);
      if (sbox->child[1]) {
	update_list(u1234, nu1234, x1234, y1234, sbox->child[1], -1, 2);
	update_list(n56, nn56, x56, y56, sbox->child[1], 2, -1);
	update_list(w8, nw8, x8, y8, sbox->child[1], -2, 2);
      }
      if (sbox->child[2]) 
	update_list(nall, nnall, xnall, ynall, sbox->child[2], 2, -2);
      if (sbox->child[3])
	update_list(nall, nnall, xnall, ynall, sbox->child[3], 2, -1);
      if (sbox->child[4]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[4], -2, 2);
      if (sbox->child[5])
	update_list(uall, nuall, xuall, yuall, sbox->child[5], -1, 2);
      if (sbox->child[6]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[6], -2, 3);
      if (sbox->child[7])
	update_list(uall, nuall, xuall, yuall, sbox->child[7], -1, 3);
      break;
    case 25: // [0][1][1], update u1234, nall, n56, nall lists
      if (sbox->child[0]) {
	update_list(u1234, nu1234, x1234, y1234,  sbox->child[0], 0, 2);
	update_list(n56, nn56, x56, y56,  sbox->child[0], 2, 0);
      }
      if (sbox->child[1]) {
	update_list(u1234, nu1234, x1234, y1234,  sbox->child[1],1, 2);
	update_list(n56, nn56, x56, y56,  sbox->child[1], 2, 1);
      }
      if (sbox->child[2]) 
	update_list(nall, nnall, xnall, ynall,  sbox->child[2], 2, 0);
      if (sbox->child[3])
	update_list(nall, nnall, xnall, ynall,  sbox->child[3], 2, 1);
      if (sbox->child[4]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[4], 0, 2);
      if (sbox->child[5])
	update_list(uall, nuall, xuall, yuall, sbox->child[5], 1, 2);
      if (sbox->child[6]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[6], 0, 3);
      if (sbox->child[7])
	update_list(uall, nuall, xuall, yuall, sbox->child[7], 1, 3);
      break;
    case 26: // [1][1][1], update u1234, n56, e7, eall, nall, uall lists 
      if (sbox->child[0]) {
	update_list(u1234, nu1234, x1234, y1234, sbox->child[0], 2, 2);
	update_list(n56, nn56, x56, y56, sbox->child[0], 2, 2);
	update_list(e7, ne7, x7, y7, sbox->child[0], -2, 2);
      }
      if (sbox->child[1]) 
	update_list(eall, neall, xeall, yeall, sbox->child[1], -2, 2);
      if (sbox->child[2]) 
	update_list(nall, nnall, xnall, ynall, sbox->child[2], 2, 2);
      if (sbox->child[3]) 
	update_list(nall, nnall, xnall, ynall, sbox->child[3], 2, 3);
      if (sbox->child[4]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[4], 2, 2);
      if (sbox->child[5])
	update_list(uall, nuall, xuall, yuall, sbox->child[5], 3, 2);
      if (sbox->child[6]) 
	update_list(uall, nuall, xuall, yuall, sbox->child[6], 2, 3);
      if (sbox->child[7])
	update_list(uall, nuall, xuall, yuall, sbox->child[7], 3, 3);
      break;
    default:
      break;
    }
  }
}
