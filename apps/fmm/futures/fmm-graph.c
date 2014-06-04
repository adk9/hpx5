#include "fmm.h"

const int xoff[8] = {0, 1, 0, 1, 0, 1, 0, 1};
const int yoff[8] = {0, 0, 1, 1, 0, 0, 1, 1};
const int zoff[8] = {0, 0, 0, 0, 1, 1, 1, 1};

int *sswap_, *srecord_, *tswap_, *trecord_, *buffer1_, *buffer4_; 

void BuildGraph(const double *sources, int nsources, 
		const double *targets, int ntargets, int s)
{
  nslev_ = nsboxes_ = ntlev_ = ntboxes_ = 0;
  nsources_ = nsources; 
  ntargets_ = ntargets; 

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

  size_ = fmax(fmax(xmax - xmin, ymax - ymin), zmax - zmin);
  corner_[0] = (xmax + xmin - size_)*0.5; 
  corner_[1] = (ymax + ymin - size_)*0.5;
  corner_[2] = (zmax + zmin - size_)*0.5; 

  mapsrc_ = calloc(nsources_, sizeof(int));
  maptar_ = calloc(ntargets_, sizeof(int));
  sswap_ = calloc(nsources_, sizeof(int));
  tswap_ = calloc(ntargets_, sizeof(int));
  srecord_ = calloc(nsources_, sizeof(int));
  trecord_ = calloc(ntargets_, sizeof(int));  
  
  for (int i = 0; i < nsources; i++) 
    mapsrc_[i] = i; 

  for (int i = 0; i < ntargets; i++) 
    maptar_[i] = i; 

  sboxes_[0] = calloc(1, sizeof(Box));
  sboxes_[0][0] = (Box) {0, ++nsboxes_, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 
			 0, 0, 0, 0, nsources, 0, 0, 0, 0, 0}; 
  
  tboxes_[0] = calloc(1, sizeof(Box)); 
  tboxes_[0][0] = (Box) {0, ++ntboxes_, 0, {0, 0, 0, 0, 0, 0, 0, 0},
			 0, 0, 0, 0, ntargets, 0, 0, 0, 0, 0};
  tboxes_[0][0].list5 = calloc(2, sizeof(int));
  tboxes_[0][0].list5[0] = 1;
  tboxes_[0][0].list5[1] = 1;

  int ns = 1, nt = 1; 
  double h = size_; 

  for (int lev = 0; lev <= MAXLEV; lev++) {
    if (lev == MAXLEV) {
      fprintf(stderr, "Too many levels of partitions have been attempted.\n");
      exit(-1);
    }

    int sbox0 = sboxes_[lev][0].boxid, mp = 0; 

    for (int ibox = 0; ibox < nt; ibox++) {
      if (tboxes_[lev][ibox].npts > s && tboxes_[lev][ibox].list5) {
	int *list5 = &tboxes_[lev][ibox].list5[1]; 
	int nlist5 = tboxes_[lev][ibox].list5[0]; 
	for (int j = 0; j < nlist5; j++) {
	  int index = list5[j] - sbox0; 
	  if (sboxes_[lev][index].npts > s) {
	    tboxes_[lev][ibox].nchild = 1; 
	    sboxes_[lev][index].nchild = 1; 
	    mp = 1; 
	  }
	}
      }
    }

    h /= 2; 
    
    if (mp) {
      for (int ibox = 0; ibox < ns; ibox++) {
	if (sboxes_[lev][ibox].nchild) 
	  PartitionBox(&sboxes_[lev][ibox], sources, h, 'S');
      }

      int nns = 0; 
      for (int ibox = 0; ibox < ns; ibox++) 
	nns += sboxes_[lev][ibox].nchild; 

      sboxes_[lev + 1] = calloc(nns, sizeof(Box)); 

      int iter = 0; 
      for (int ibox = 0; ibox < ns; ibox++) {
	if (sboxes_[lev][ibox].nchild) {
	  int offset = 0; 
	  for (int i = 0; i < 8; i++) {
	    if (sboxes_[lev][ibox].child[i]) {
	      sboxes_[lev + 1][iter].level = lev + 1; 
	      sboxes_[lev + 1][iter].boxid = ++nsboxes_; 
	      sboxes_[lev + 1][iter].parent = sboxes_[lev][ibox].boxid; 
	      sboxes_[lev + 1][iter].idx = 2*sboxes_[lev][ibox].idx + xoff[i]; 
	      sboxes_[lev + 1][iter].idy = 2*sboxes_[lev][ibox].idy + yoff[i]; 
	      sboxes_[lev + 1][iter].idz = 2*sboxes_[lev][ibox].idz + zoff[i]; 
	      sboxes_[lev + 1][iter].npts = sboxes_[lev][ibox].child[i]; 
	      sboxes_[lev + 1][iter].addr = sboxes_[lev][ibox].addr + offset; 
	      offset += sboxes_[lev + 1][iter].npts; 
	      sboxes_[lev][ibox].child[i] = sboxes_[lev + 1][iter].boxid; 
	      iter++; 
	    }
	  }
	}
      }

      ns = nns; 
    } else {
      nslev_ = lev; 
    }

    if (mp) {
      for (int ibox = 0; ibox < nt; ibox++) {
	if (tboxes_[lev][ibox].nchild) 
	  PartitionBox(&tboxes_[lev][ibox], targets, h, 'T');
      }

      int nnt = 0; 
      for (int ibox = 0; ibox < nt; ibox++) 
	nnt += tboxes_[lev][ibox].nchild; 

      tboxes_[lev + 1] = calloc(nnt, sizeof(Box)); 

      int iter = 0; 
      for (int ibox = 0; ibox < nt; ibox++) {
	if (tboxes_[lev][ibox].nchild) {
	  int offset = 0; 
	  for (int i = 0; i < 8; i++) {
	    if (tboxes_[lev][ibox].child[i]) {
	      tboxes_[lev + 1][iter].level = lev + 1;
	      tboxes_[lev + 1][iter].boxid = ++ntboxes_; 
	      tboxes_[lev + 1][iter].parent = tboxes_[lev][ibox].boxid; 
	      tboxes_[lev + 1][iter].idx = 2*tboxes_[lev][ibox].idx + xoff[i]; 
	      tboxes_[lev + 1][iter].idy = 2*tboxes_[lev][ibox].idy + yoff[i]; 
	      tboxes_[lev + 1][iter].idz = 2*tboxes_[lev][ibox].idz + zoff[i]; 
	      tboxes_[lev + 1][iter].npts = tboxes_[lev][ibox].child[i]; 
	      tboxes_[lev + 1][iter].addr = tboxes_[lev][ibox].addr + offset; 
	      offset += tboxes_[lev + 1][iter].npts; 
	      tboxes_[lev][ibox].child[i] = tboxes_[lev + 1][iter].boxid; 
	      iter++; 
	    }
	  }
	}
      }


      for (int ibox = 0; ibox < nnt; ibox++) {
	int nlist5 = 0, temp5[27] = {0}; 
	int tidx = tboxes_[lev + 1][ibox].idx;
	int tidy = tboxes_[lev + 1][ibox].idy; 
	int tidz = tboxes_[lev + 1][ibox].idz; 
	int index = tboxes_[lev + 1][ibox].parent - tboxes_[lev][0].boxid; 
	int *plist5 = &tboxes_[lev][index].list5[1]; 
	int nplist5 = tboxes_[lev][index].list5[0]; 
	for (int j = 0; j < nplist5; j++) {
	  index = plist5[j] - sbox0; 
	  for (int iter = 0; iter < 8; iter++) {
	    int child = sboxes_[lev][index].child[iter]; 
	    if (child) {
	      int index1 = child - sboxes_[lev + 1][0].boxid; 
	      int sidx = sboxes_[lev + 1][index1].idx; 
	      int sidy = sboxes_[lev + 1][index1].idy;
	      int sidz = sboxes_[lev + 1][index1].idz; 
	      int diffx = fabs(tidx - sidx) <= 1;
	      int diffy = fabs(tidy - sidy) <= 1;
	      int diffz = fabs(tidz - sidz) <= 1;
	      if (diffx*diffy*diffz) 
		temp5[nlist5++] = child;
	    }
	  }
	}


	tboxes_[lev + 1][ibox].list5 = calloc(1 + nlist5, sizeof(int)); 

	tboxes_[lev + 1][ibox].list5[0] = nlist5; 
	memcpy(&tboxes_[lev + 1][ibox].list5[1], temp5, sizeof(int)*nlist5); 
      }

      nt = nnt; 
    } else {
      ntlev_ = lev; 
      break;
    }
  }

  sboxptrs_ = calloc(1 + nsboxes_, sizeof(Box *)); 
  tboxptrs_ = calloc(1 + ntboxes_, sizeof(Box *)); 

  int begin, end; 
  for (int lev = 0; lev < nslev_; lev++) {
    begin = sboxes_[lev][0].boxid; 
    end = sboxes_[lev + 1][0].boxid - 1; 
    for (int j = begin; j <= end; j++) 
      sboxptrs_[j] = &sboxes_[lev][j - begin]; 
  }

  begin = sboxes_[nslev_][0].boxid; 
  end = nsboxes_; 
  for (int j = begin; j <= end; j++) 
    sboxptrs_[j] = &sboxes_[nslev_][j - begin]; 

  for (int lev = 0; lev < ntlev_; lev++) {
    begin = tboxes_[lev][0].boxid; 
    end = tboxes_[lev + 1][0].boxid - 1; 
    for (int j = begin; j <= end; j++) 
      tboxptrs_[j] = &tboxes_[lev][j - begin]; 
  }

  begin = tboxes_[nslev_][0].boxid; 
  end = ntboxes_; 
  for (int j = begin; j <= end; j++) 
    tboxptrs_[j] = &tboxes_[nslev_][j - begin]; 

  free(sswap_);
  free(srecord_);
  free(tswap_);
  free(trecord_);
    

  buffer1_ = calloc(27*(ntboxes_ + 1), sizeof(int));
  buffer4_ = calloc(24*(ntboxes_ + 1), sizeof(int)); 

  for (int i = 0; i < 8; i++) {
    int child = tboxes_[0][0].child[i]; 
    if (child) 
      BuildList134(tboxptrs_[child]); 
  }

  free(buffer1_);
  free(buffer4_);
}


void PartitionBox(Box *ibox, const double *points, double h, char tag)
{
  int npoints = ibox->npts;
  int begin = ibox->addr;

  int *imap = (tag == 'S' ? &mapsrc_[begin] : &maptar_[begin]);
  int *swap = (tag == 'S' ? &sswap_[begin] : &tswap_[begin]);
  int *record = (tag == 'S' ? &srecord_[begin] : &trecord_[begin]);

  double xc = corner_[0] + (2*ibox->idx + 1)*h; 
  double yc = corner_[1] + (2*ibox->idy + 1)*h;
  double zc = corner_[2] + (2*ibox->idz + 1)*h; 

  int addrs[8] = {0}, assigned[8] = {0}; 

  for (int i = 0; i < npoints; i++) {
    int j = 3*imap[i]; 
    int bin = 4*(points[j + 2] > zc) + 2*(points[j + 1] > yc) + (points[j] > xc); 
    ibox->child[bin]++;
    record[i] = bin;
  }

  for (int i = 1; i < 8; i++) 
    addrs[i] = addrs[i - 1] + ibox->child[i - 1]; 

  for (int i = 0; i < npoints; i++) {
    int bin = record[i]; 
    int offset = addrs[bin] + assigned[bin]; 
    swap[offset] = imap[i]; 
    assigned[bin]++;
  }

  for (int i = 0; i < npoints; i++) 
    imap[i] = swap[i]; 

  ibox->nchild = 0; 
  for (int i = 0; i < 8; i++)
    ibox->nchild += ibox->child[i] > 0;
}    
    

void BuildList134(Box *tbox)
{
  int nlist1 = 0, nlist4 = 0; 
  int *temp1 = &buffer1_[27*tbox->boxid], *temp4 = &buffer4_[24*tbox->boxid]; 
  Box *parent = tboxptrs_[tbox->parent]; 
  int *plist1 = (parent->list1 ? &parent->list1[1] : 0);
  int nplist1 = (plist1 ? parent->list1[0] : 0); 
  
  for (int iter = 0; iter < nplist1; iter++) {
    Box *sbox = sboxptrs_[plist1[iter]]; 
    if (IsAdjacent(sbox, tbox)) {
      temp1[nlist1++] = sbox->boxid; 
    } else {
      temp4[nlist4++] = sbox->boxid; 
    }
  }

  if (nlist4) {
   tbox->list4 = calloc(1 + nlist4, sizeof(int)); 

    tbox->list4[0] = nlist4; 
    int *list4 = &tbox->list4[1]; 
    memcpy(&tbox->list4[1], temp4, sizeof(int)*nlist4); 
  }

  if (tbox->nchild) {
    int *list5 = &tbox->list5[1]; 
    int nlist5 = tbox->list5[0]; 
    for (int j = 0; j < nlist5; j++) {
      Box *sbox = sboxptrs_[list5[j]]; 
      if (!sbox->nchild) 
	temp1[nlist1++] = sbox->boxid; 
    }

    if (nlist1) {
      tbox->list1 = calloc(1 + nlist1, sizeof(int)); 

      tbox->list1[0] = nlist1; 
      int *list1 = &tbox->list1[1]; 
      memcpy(&tbox->list1[1], temp1, sizeof(int)*nlist1); 
    }

    for (int j = 0; j < 8; j++) {
      int child = tbox->child[j]; 
      if (child) 
	BuildList134(tboxptrs_[child]); 
    }
  } else {
    if (tbox->list5) 
      BuildList13(tbox, temp1, nlist1); 
  }  
}

void BuildList13(Box *tbox, const int *coarse_list, int ncoarse_list)
{
  int *list5 = &tbox->list5[1]; 
  int nlist5 = tbox->list5[0]; 
  int level = tbox->level; 
  int M = nslev_ - level, M2 = 1<<M, M4 = M2*M2; 
  int nlist1 = 0, nlist3 = 0; 
  int bufsz1 = nlist5*M4, bufsz3 = 8*M4 + 72*M2 + 56*M - 80; 
  int *temp1 = calloc(bufsz1, sizeof(int)); 
  int *temp3 = calloc(bufsz3, sizeof(int)); 

  for (int j = 0; j < nlist5; j++) {
    Box *sbox = sboxptrs_[list5[j]]; 
    BuildList13FromBox(tbox, sbox, temp1, &nlist1, temp3, &nlist3); 
  }

 if (nlist1 + ncoarse_list) {
    tbox->list1 = calloc(1 + nlist1 + ncoarse_list, sizeof(int)); 

    tbox->list1[0] = nlist1 + ncoarse_list; 
    int *list1 = &tbox->list1[1]; 

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

void BuildList13FromBox (Box *tbox, Box *sbox, int *list1, int *nlist1, 
			 int *list3, int *nlist3)
{
  if (IsAdjacent(tbox, sbox)) {
    if (sbox->nchild) {
      for (int j = 0; j < 8; j++) {
	int child = sbox->child[j]; 
	if (child) 
	  BuildList13FromBox(tbox, sboxptrs_[child], list1, nlist1, list3, nlist3);
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

int IsAdjacent(const Box *box1, const Box *box2)
{
  int dim = pow(2, box2->level - box1->level); 
  int dim1 = dim + 1; 

  return ((box2->idx >= dim*box1->idx - 1) && (box2->idx <= dim*box1->idx + dim) &&
	  (box2->idy >= dim*box1->idy - 1) && (box2->idy <= dim*box1->idy + dim) &&
	  (box2->idz >= dim*box1->idz - 1) && (box2->idz <= dim*box1->idz + dim)); 
}

void DestroyGraph(void) 
{
  for (int lev = 0; lev <= nslev_; lev++) 
    free(sboxes_[lev]);

  free(sboxptrs_);
  free(mapsrc_);

  for (int i = 1; i <= ntboxes_; i++) {
    Box *ibox = tboxptrs_[i]; 
    free(ibox->list1);
    free(ibox->list3);
    free(ibox->list4);
    free(ibox->list5);
  }

  for (int lev = 0; lev <= ntlev_; lev++) 
    free(tboxes_[lev]); 

  free(tboxptrs_);
  free(maptar_);    
}

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
		      int *w8, int *nw8, int *x8, int *y8)
{
  *nuall = *nu1234 = 0;
  *ndall = *nd5678 = 0;
  *nnall = *nn1256 = *nn12 = *nn56 = 0;
  *nsall = *ns3478 = *ns34 = *ns78 = 0;
  *neall = *ne1357 = *ne13 = *ne57 = *ne1 = *ne3 = *ne5 = *ne7 = 0;
  *nwall = *nw2468 = *nw24 = *nw68 = *nw2 = *nw4 = *nw6 = *nw8 = 0;

  int tidx = ibox->idx, tidy = ibox->idy, tidz = ibox->idz; 
  int *list5 = &ibox->list5[1]; 
  int nlist5 = ibox->list5[0]; 

  for (int iter = 0; iter < nlist5; iter++) {
    Box *sbox = sboxptrs_[list5[iter]]; 
    int sidx = sbox->idx, sidy = sbox->idy, sidz = sbox->idz; 
    int offset = 9*(sidz - tidz) + 3*(sidy - tidy) + sidx - tidx + 13; 
    switch (offset) {
    case 0:
      // [-1][-1][-1], update dall, sall, wall, d5678, s34, w2 lists
      if (sbox->child[0]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[0], -2, -2);
      if (sbox->child[1]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[1], -1,-2);
      if (sbox->child[2]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[2], -2, -1);
      if (sbox->child[3]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[3], -1,-1);
      if (sbox->child[4]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[4], -1, -2);
      if (sbox->child[5]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[5], -1, -1);
      if (sbox->child[6]) 
	UpdateList(wall, nwall, xwall, ywall, sbox->child[6], 1, -1);
      if (sbox->child[7]) {
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[7], -1, -1);
	UpdateList(s34, ns34, x34, y34, sbox->child[7], -1, -1);
	UpdateList(w2, nw2, x2, y2, sbox->child[7], 1, -1);
      }
      break;
    case 1://[0][-1][-1], update dall, sall, d5678, s34 lists
      if (sbox->child[0]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[0], 0, -2);
      if (sbox->child[1]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[1], 1, -2);
      if (sbox->child[2]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[2], 0, -1);
      if (sbox->child[3]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[3], 1,-1);
      if (sbox->child[4]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[4], -1, 0);
      if (sbox->child[5]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[5], -1, 1);     
      if (sbox->child[6]) {
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[6], 0, -1);
	UpdateList(s34, ns34, x34, y34, sbox->child[6], -1,0);
      }
      if (sbox->child[7]) {
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[7], 1, -1);
	UpdateList(s34, ns34, x34, y34, sbox->child[7], -1, 1);
      }
      break;
    case 2: //[1][-1][-1], update dall, sall, eall, d5678, s34, e1 lists
      if (sbox->child[0]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[0], 2, -2);
      if (sbox->child[1]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[1], 3, -2);
      if (sbox->child[2]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[2], 2, -1);
      if (sbox->child[3]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[3], 3, -1);
      if (sbox->child[4]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[4], -1, 2);
      if (sbox->child[5]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[5], -1, 3);
      if (sbox->child[6]) {
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[6], 2, -1);
	UpdateList(s34, ns34, x34, y34, sbox->child[6], -1, 2);
	UpdateList(e1, ne1, x1, y1, sbox->child[6], 1, -1);
      }
      if (sbox->child[7]) 
	UpdateList(eall, neall, xeall, yeall, sbox->child[7], 1, -1);
      break;
    case 3: //[-1][0][-1], update dall, wall, d5678, w24 lists
      if (sbox->child[0]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[0], -2, 0);
      if (sbox->child[1]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[1], -1, 0);
      if (sbox->child[2]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[2], -2, 1);
      if (sbox->child[3]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[3], -1, 1);
      if (sbox->child[4]) 
	UpdateList(wall, nwall, xwall, ywall, sbox->child[4], 1, 0);
      if (sbox->child[5]) {
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[5], -1, 0);
	UpdateList(w24, nw24, x24, y24, sbox->child[5], 1, 0);
      }
      if (sbox->child[6]) 
	UpdateList(wall, nwall, xwall, ywall, sbox->child[6], 1, 1);
      if (sbox->child[7]) {
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[7], -1, 1);
	UpdateList(w24, nw24, x24, y24, sbox->child[7], 1, 1);
      }
      break;
    case 4://[0][0][-1], update dall and d5678 lists
      if (sbox->child[0]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[0], 0, 0);
      if (sbox->child[1]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[1], 1, 0);
      if (sbox->child[2]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[2], 0, 1);
      if (sbox->child[3]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[3], 1, 1);
      if (sbox->child[4]) 
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[4], 0, 0);
      if (sbox->child[5]) 
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[5], 1, 0);
      if (sbox->child[6]) 
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[6], 0, 1);
      if (sbox->child[7]) 
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[7], 1, 1);
      break;
    case 5: // [1][0][-1], update dall, d5678, eall, e13 lists
      if (sbox->child[0]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[0], 2, 0);
      if (sbox->child[1]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[1], 3, 0);
      if (sbox->child[2]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[2], 2, 1);
      if (sbox->child[3]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[3], 3, 1);
      if (sbox->child[4]) {
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[4], 2, 0);
	UpdateList(e13, ne13, x13, y13, sbox->child[4], 1, 0);
      }
      if (sbox->child[5]) 
	UpdateList(eall, neall, xeall, yeall, sbox->child[5], 1, 0);
      if (sbox->child[6]) {
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[6], 2, 1);
	UpdateList(e13, ne13, x13, y13, sbox->child[6], 1, 1);
      }
      if (sbox->child[7]) 
	UpdateList(eall, neall, xeall, yeall, sbox->child[7], 1, 1);
      break;
    case 6://[-1][1][-1], update dall, nall, wall, d5678, n12, w4 list3
      if (sbox->child[0]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[0], -2, 2);
      if (sbox->child[1]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[1], -1, 2);
      if (sbox->child[2]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[2], -2, 3);
      if (sbox->child[3]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[3], -1, 3);
      if (sbox->child[4]) 
	UpdateList(wall, nwall, xwall, ywall, sbox->child[4], 1, 2);
      if (sbox->child[5]) {
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[5], -1, 2);
	UpdateList(n12, nn12, x12, y12, sbox->child[5], -1, -1);
	UpdateList(w4, nw4, x4, y4, sbox->child[5], 1, 2);
      }
      if (sbox->child[6]) 
	UpdateList(nall, nnall, xnall, ynall, sbox->child[6], -1, -2);
      if (sbox->child[7]) 
	UpdateList(nall, nnall, xnall, ynall, sbox->child[7], -1, -1);
      break;
    case 7: //[0][1][-1], update dallm d5678, nall, n12 lists 
      if (sbox->child[0]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[0], 0, 2);
      if (sbox->child[1]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[1], 1, 2);
      if (sbox->child[2]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[2], 0, 3);
      if (sbox->child[3]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[3], 1, 3);
      if (sbox->child[4]) {
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[4], 0, 2);
	UpdateList(n12, nn12, x12, y12, sbox->child[4], -1, 0);
      }
      if (sbox->child[5]) {
	UpdateList(d5678, nd5678,x5678, y5678, sbox->child[5], 1, 2);
	UpdateList(n12, nn12, x12, y12, sbox->child[5], -1, 1);
      }
      if (sbox->child[6]) 
	UpdateList(nall, nnall, xnall, ynall, sbox->child[6], -1, 0);
      if (sbox->child[7])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[7], -1, 1);
      break;
    case 8: //[1][1][-1], update dall, d5678, nall, eall, n12, e3 lists
      if (sbox->child[0]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[0], 2,2);
      if (sbox->child[1]) 
	UpdateList(dall, ndall, xdall, ydall, sbox->child[1], 3, 2);
      if (sbox->child[2])
	UpdateList(dall, ndall, xdall, ydall, sbox->child[2], 2, 3);
      if (sbox->child[3])
	UpdateList(dall, ndall, xdall, ydall, sbox->child[3], 3,3);
      if (sbox->child[4]) {
	UpdateList(d5678, nd5678, x5678, y5678, sbox->child[4], 2, 2);
	UpdateList(n12, nn12, x12, y12, sbox->child[4], -1, 2);
	UpdateList(e3, ne3, x3, y3, sbox->child[4], 1, 2);
      }
      if (sbox->child[5]) 
	UpdateList(eall, neall, xeall, yeall, sbox->child[5], 1, 2);
      if (sbox->child[6]) 
	UpdateList(nall, nnall, xnall, ynall, sbox->child[6], -1, 2);
      if (sbox->child[7]) 
	UpdateList(nall, nnall, xnall, ynall, sbox->child[7], -1, 3);
      break;
    case 9: // [-1][-1][0], update sall, wall, s3478, w2, w6 lists
      if (sbox->child[0]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[0], 0, -2);
      if (sbox->child[1])
	UpdateList(sall, nsall, xsall, ysall, sbox->child[1], 0, -1);
      if (sbox->child[2])
	UpdateList(wall, nwall, xwall, ywall, sbox->child[2], 0, -1);
      if (sbox->child[3]) {
	UpdateList(s3478, ns3478, x3478, y3478, sbox->child[3], 0, -1);
	UpdateList(w2, nw2, x2, y2, sbox->child[3], 0, -1);
	UpdateList(w6, nw6, x6, y6, sbox->child[3], 0, -1);
      }
      if (sbox->child[4]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[4], 1, -2);
      if (sbox->child[5])
	UpdateList(sall, nsall, xsall, ysall, sbox->child[5], 1, -1);
      if (sbox->child[6]) 
	UpdateList(wall, nwall, xwall, ywall, sbox->child[6], -1, -1);
      if (sbox->child[7]) {
	UpdateList(s3478, ns3478, x3478, y3478, sbox->child[7], 1, -1);
	UpdateList(w2, nw2, x2, y2, sbox->child[7], -1, -1);
	UpdateList(w6, nw6, x6, y6, sbox->child[7], -1, -1);
      }
      break;
    case 10://[0][-1][0], update sall, s3478 lists
      if (sbox->child[0]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[0], 0, 0);
      if (sbox->child[1])
	UpdateList(sall, nsall, xsall, ysall, sbox->child[1], 0, 1);
      if (sbox->child[2])
	UpdateList(s3478, ns3478, x3478, y3478, sbox->child[2], 0, 0);
      if (sbox->child[3])
	UpdateList(s3478, ns3478, x3478, y3478, sbox->child[3], 0, 1);
      if (sbox->child[4]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[4], 1, 0);
      if (sbox->child[5])
	UpdateList(sall, nsall, xsall, ysall, sbox->child[5], 1, 1);
      if (sbox->child[6])
	UpdateList(s3478, ns3478, x3478, y3478, sbox->child[6], 1, 0);
      if (sbox->child[7])
	UpdateList(s3478, ns3478, x3478, y3478, sbox->child[7], 1, 1);
      break;
    case 11: //[1][-1][0], update eall, sall, s3478, e1, e5 lists
      if (sbox->child[0])
	UpdateList(sall, nsall, xsall, ysall, sbox->child[0], 0, 2);
      if (sbox->child[1])
	UpdateList(sall, nsall, xsall, ysall, sbox->child[1], 0, 3);
      if (sbox->child[2]) {
	UpdateList(s3478, ns3478, x3478, y3478, sbox->child[2], 0, 2);
	UpdateList(e1, ne1, x1, y1, sbox->child[2], 0, -1);
	UpdateList(e5, ne5, x5, y5, sbox->child[2], 0, -1);
      }
      if (sbox->child[3]) 
	UpdateList(eall, neall, xeall, yeall, sbox->child[3], 0, -1);
      if (sbox->child[4])
	UpdateList(sall, nsall, xsall, ysall, sbox->child[4], 1, 2);
      if (sbox->child[5]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[5], 1, 3);
      if (sbox->child[6]) {
	UpdateList(s3478, ns3478, x3478, y3478, sbox->child[6], 1, 2);
	UpdateList(e1, ne1, x1, y1, sbox->child[6], -1, -1);
	UpdateList(e5, ne5, x5, y5, sbox->child[6], -1, -1);
      }
      if (sbox->child[7]) 
	UpdateList(eall, neall, xeall, yeall, sbox->child[7], -1, -1);
      break;
    case 12: // [-1][0][0], update wall, w2468 lists
      if (sbox->child[0])
	UpdateList(wall, nwall, xwall, ywall, sbox->child[0], 0, 0);
      if (sbox->child[1])
	UpdateList(w2468, nw2468, x2468, y2468, sbox->child[1], 0, 0);
      if (sbox->child[2])
	UpdateList(wall, nwall, xwall, ywall, sbox->child[2], 0, 1);
      if (sbox->child[3])
	UpdateList(w2468, nw2468, x2468, y2468, sbox->child[3], 0, 1);
      if (sbox->child[4])
	UpdateList(wall, nwall, xwall, ywall, sbox->child[4], -1, 0);
      if (sbox->child[5])
	UpdateList(w2468, nw2468, x2468, y2468, sbox->child[5], -1, 0);
      if (sbox->child[6])
	UpdateList(wall, nwall, xwall, ywall, sbox->child[6], -1, 1);
      if (sbox->child[7])
	UpdateList(w2468, nw2468, x2468, y2468, sbox->child[7], -1, 1);
      break;
    case 13: //[0][0][0], nothing here
      break;
    case 14: //[1][0][0], update eall, e1357 lists
      if (sbox->child[0])
	UpdateList(e1357, ne1357, x1357, y1357, sbox->child[0], 0, 0);
      if (sbox->child[1])
	UpdateList(eall, neall, xeall, yeall, sbox->child[1], 0, 0);
      if (sbox->child[2])
	UpdateList(e1357, ne1357, x1357, y1357, sbox->child[2], 0, 1);
      if (sbox->child[3])
	UpdateList(eall, neall, xeall, yeall, sbox->child[3], 0, 1);
      if (sbox->child[4]) 
	UpdateList(e1357, ne1357, x1357, y1357, sbox->child[4], -1, 0);
      if (sbox->child[5])
	UpdateList(eall, neall, xeall, yeall, sbox->child[5], -1, 0);
      if (sbox->child[6])
	UpdateList(e1357, ne1357, x1357, y1357, sbox->child[6], -1, 1);
      if (sbox->child[7])
	UpdateList(eall, neall, xeall, yeall, sbox->child[7], -1, 1);
      break;
    case 15://[-1][1][0], update wall, nall, n1256, w4, w8 lists
      if (sbox->child[0])
	UpdateList(wall, nwall, xwall, ywall, sbox->child[0], 0, 2);
      if (sbox->child[1]) {
	UpdateList(n1256, nn1256, x1256, y1256, sbox->child[1], 0, -1);
	UpdateList(w4, nw4, x4, y4, sbox->child[1], 0, 2);
	UpdateList(w8, nw8, x8, y8, sbox->child[1], 0, 2);
      }
      if (sbox->child[2])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[2], 0, -2);
      if (sbox->child[3])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[3], 0, -1);
      if (sbox->child[4])
	UpdateList(wall, nwall, xwall, ywall, sbox->child[4], -1, 2);
      if (sbox->child[5]) {
	UpdateList(n1256, nn1256, x1256, y1256, sbox->child[5], 1, -1);
	UpdateList(w4, nw4, x4, y4, sbox->child[5], -1, 2);
	UpdateList(w8, nw8, x8, y8, sbox->child[5], -1, 2);
      }
      if (sbox->child[6]) 
	UpdateList(nall, nnall, xnall, ynall, sbox->child[6], 1, -2);
      if (sbox->child[7])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[7], 1, -1);
      break;
    case 16: //[0][1][0], update nall, n1256 lists
      if (sbox->child[0])
	UpdateList(n1256, nn1256, x1256, y1256, sbox->child[0], 0, 0);
      if (sbox->child[1])
	UpdateList(n1256, nn1256, x1256, y1256, sbox->child[1], 0, 1);
      if (sbox->child[2])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[2], 0, 0);
      if (sbox->child[3])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[3], 0, 1);
      if (sbox->child[4])
	UpdateList(n1256, nn1256, x1256, y1256, sbox->child[4], 1, 0);
      if (sbox->child[5])
	UpdateList(n1256, nn1256, x1256, y1256, sbox->child[5], 1, 1);
      if (sbox->child[6])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[6], 1, 0);
      if (sbox->child[7])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[7], 1, 1);
      break;
    case 17: //[1][1][0], update nall, n1256, eall, e3, e7 lists
      if (sbox->child[0]) {
	UpdateList(n1256, nn1256, x1256, y1256, sbox->child[0], 0, 2);
	UpdateList(e3, ne3, x3, y3, sbox->child[0], 0, 2);
	UpdateList(e7, ne7, x7, y7, sbox->child[0], 0, 2);
      }
      if (sbox->child[1]) 
	UpdateList(eall, neall, xeall, yeall, sbox->child[1], 0, 2);
      if (sbox->child[2])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[2], 0, 2);
      if (sbox->child[3])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[3], 0, 3);
      if (sbox->child[4]) {
	UpdateList(n1256, nn1256, x1256, y1256, sbox->child[4], 1, 2);
	UpdateList(e3, ne3, x3, y3, sbox->child[4], -1, 2);
	UpdateList(e7, ne7, x7, y7, sbox->child[4], -1, 2);
      }
      if (sbox->child[5])
	UpdateList(eall, neall, xeall, yeall, sbox->child[5], -1, 2);
      if (sbox->child[6])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[6], 1, 2);
      if (sbox->child[7])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[7], 1, 3);
      break;
    case 18: //[-1][-1][1], update sall, wall, u1234, s78, w6, uall lists
      if (sbox->child[0]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[0], 2, -2);
      if (sbox->child[1]) 
	UpdateList(sall, nsall, xsall, ysall, sbox->child[1], 2, -1);
      if (sbox->child[2]) 
	UpdateList(wall, nwall, xwall, ywall, sbox->child[2], -2, -1);
      if (sbox->child[3]) {
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[3], -1,-1);
	UpdateList(s78, ns78, x78, y78, sbox->child[3], 2, -1);
	UpdateList(w6, nw6, x6, y6, sbox->child[3], -2, -1);
      }
      if (sbox->child[4]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[4], -2, -2);
      if (sbox->child[5])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[5], -1, -2);
      if (sbox->child[6])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[6], -2, -1);
      if (sbox->child[7])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[7], -1, -1);
      break;
    case 19: //[0][-1][1], update sall, u1234, s78, uall lists
      if (sbox->child[0])
	UpdateList(sall, nsall, xsall, ysall, sbox->child[0], 2, 0);
      if (sbox->child[1])
	UpdateList(sall, nsall, xsall, ysall, sbox->child[1], 2, 1);
      if (sbox->child[2]) {
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[2], 0, -1);
	UpdateList(s78, ns78, x78, y78, sbox->child[2], 2, 0);
      }
      if (sbox->child[3]) {
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[3], 1, -1);
	UpdateList(s78, ns78, x78, y78, sbox->child[3], 2, 1);
      }
      if (sbox->child[4]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[4], 0, -2);
      if (sbox->child[5])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[5], 1, -2);
      if (sbox->child[6])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[6], 0, -1);
      if (sbox->child[7])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[7], 1, -1);
      break;
    case 20: // [1][-1][1], update sall, eall, u1234, s78, e5, uall lists
      if (sbox->child[0])
	UpdateList(sall, nsall, xsall, ysall, sbox->child[0], 2, 2);
      if (sbox->child[1])
	UpdateList(sall, nsall, xsall, ysall, sbox->child[1], 2, 3);
      if (sbox->child[2]) {
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[2], 2, -1);
	UpdateList(s78, ns78, x78, y78, sbox->child[2], 2, 2);
	UpdateList(e5, ne5, x5, y5, sbox->child[2], -2, -1);
      }
      if (sbox->child[3])
	UpdateList(eall, neall, xeall, yeall, sbox->child[3], -2, -1);
      if (sbox->child[4])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[4], 2, -2);
      if (sbox->child[5])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[5], 3, -2);
      if (sbox->child[6])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[6], 2, -1);
      if (sbox->child[7])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[7], 3, -1);
      break;
    case 21: // [-1][0][1], update wall, u1234, w68, uall lists
      if (sbox->child[0])
	UpdateList(wall, nwall, xwall, ywall, sbox->child[0], -2, 0);
      if (sbox->child[1]) {
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[1], -1, 0);
	UpdateList(w68, nw68, x68, y68, sbox->child[1], -2, 0);
      }
      if (sbox->child[2]) 
	UpdateList(wall, nwall, xwall, ywall, sbox->child[2], -2, 1);
      if (sbox->child[3]) {
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[3], -1, 1);
	UpdateList(w68, nw68, x68, y68, sbox->child[3], -2, 1);
      }
      if (sbox->child[4]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[4], -2, 0);
      if (sbox->child[5])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[5], -1, 0);
      if (sbox->child[6])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[6], -2, 1);
      if (sbox->child[7])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[7], -1, 1);
      break;
    case 22: //[0][0][1], update u1234, uall lists
      if (sbox->child[0]) 
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[0], 0, 0);
      if (sbox->child[1])
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[1], 1, 0);
      if (sbox->child[2]) 
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[2], 0, 1);
      if (sbox->child[3])
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[3], 1, 1);
      if (sbox->child[4]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[4], 0, 0);
      if (sbox->child[5])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[5], 1, 0);
      if (sbox->child[6]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[6], 0, 1);
      if (sbox->child[7])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[7], 1, 1);
      break;
    case 23: // [1][0][1], update u1234, e57, eall, uall lists
      if (sbox->child[0]) {
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[0], 2, 0);
	UpdateList(e57, ne57, x57, y57, sbox->child[0], -2, 0);
      }
      if (sbox->child[1])
	UpdateList(eall, neall, xeall, yeall, sbox->child[1], -2, 0);
      if (sbox->child[2]) {
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[2], 2, 1);
	UpdateList(e57, ne57, x57, y57, sbox->child[2], -2, 1);
      }
      if (sbox->child[3])
	UpdateList(eall, neall, xeall, yeall, sbox->child[3], -2, 1);
      if (sbox->child[4]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[4], 2, 0);
      if (sbox->child[5])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[5], 3, 0);
      if (sbox->child[6]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[6], 2, 1);
      if (sbox->child[7])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[7], 3, 1);
      break;
    case 24: // [-1][1][1], update nall, wall, u1234, n56, w8, uall lists
      if (sbox->child[0]) 
	UpdateList(wall, nwall, xwall, ywall, sbox->child[0], -2, 2);
      if (sbox->child[1]) {
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[1], -1, 2);
	UpdateList(n56, nn56, x56, y56, sbox->child[1], 2, -1);
	UpdateList(w8, nw8, x8, y8, sbox->child[1], -2, 2);
      }
      if (sbox->child[2]) 
	UpdateList(nall, nnall, xnall, ynall, sbox->child[2], 2, -2);
      if (sbox->child[3])
	UpdateList(nall, nnall, xnall, ynall, sbox->child[3], 2, -1);
      if (sbox->child[4]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[4], -2, 2);
      if (sbox->child[5])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[5], -1, 2);
      if (sbox->child[6]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[6], -2, 3);
      if (sbox->child[7])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[7], -1, 3);
      break;
    case 25: // [0][1][1], update u1234, nall, n56, nall lists
      if (sbox->child[0]) {
	UpdateList(u1234, nu1234, x1234, y1234,  sbox->child[0], 0, 2);
	UpdateList(n56, nn56, x56, y56,  sbox->child[0], 2, 0);
      }
      if (sbox->child[1]) {
	UpdateList(u1234, nu1234, x1234, y1234,  sbox->child[1],1, 2);
	UpdateList(n56, nn56, x56, y56,  sbox->child[1], 2, 1);
      }
      if (sbox->child[2]) 
	UpdateList(nall, nnall, xnall, ynall,  sbox->child[2], 2, 0);
      if (sbox->child[3])
	UpdateList(nall, nnall, xnall, ynall,  sbox->child[3], 2, 1);
      if (sbox->child[4]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[4], 0, 2);
      if (sbox->child[5])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[5], 1, 2);
      if (sbox->child[6]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[6], 0, 3);
      if (sbox->child[7])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[7], 1, 3);
      break;
    case 26: // [1][1][1], update u1234, n56, e7, eall, nall, uall lists 
      if (sbox->child[0]) {
	UpdateList(u1234, nu1234, x1234, y1234, sbox->child[0], 2, 2);
	UpdateList(n56, nn56, x56, y56, sbox->child[0], 2, 2);
	UpdateList(e7, ne7, x7, y7, sbox->child[0], -2, 2);
      }
      if (sbox->child[1]) 
	UpdateList(eall, neall, xeall, yeall, sbox->child[1], -2, 2);
      if (sbox->child[2]) 
	UpdateList(nall, nnall, xnall, ynall, sbox->child[2], 2, 2);
      if (sbox->child[3]) 
	UpdateList(nall, nnall, xnall, ynall, sbox->child[3], 2, 3);
      if (sbox->child[4]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[4], 2, 2);
      if (sbox->child[5])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[5], 3, 2);
      if (sbox->child[6]) 
	UpdateList(uall, nuall, xuall, yuall, sbox->child[6], 2, 3);
      if (sbox->child[7])
	UpdateList(uall, nuall, xuall, yuall, sbox->child[7], 3, 3);
      break;
    default:
      break;
    }
  }
}
