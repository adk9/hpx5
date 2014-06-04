#include "fmm.h"

hpx_thread_t **th_mpole; 
long *th_margs; 
hpx_future_t **fut_mpole; 

hpx_thread_t **th_expo; 
long *th_eargs; 
hpx_future_t **fut_expo; 

hpx_thread_t **th_local; 
long *th_largs; 
hpx_future_t **fut_local; 

void FMMCompute(void)
{
  hpx_config_t cfg; 

  hpx_init(); 
  hpx_config_init(&cfg); 
  int stacksize = 1 << 25; 
  hpx_config_set_thread_stack_size(&cfg, stacksize); 

  th_mpole  = hpx_alloc(sizeof(hpx_thread_t *)*(1 + nsboxes_)); 
  th_margs  = hpx_alloc(sizeof(long)*(1 + nsboxes_)); 
  fut_mpole = hpx_alloc(sizeof(hpx_future_t *)*(1 + nsboxes_)); 

  th_expo   = hpx_alloc(sizeof(hpx_thread_t *)*(1 + nsboxes_));
  th_eargs  = hpx_alloc(sizeof(long)*(1 + nsboxes_));
  fut_expo  = hpx_alloc(sizeof(hpx_future_t *)*(1 + nsboxes_));

  th_local   = hpx_alloc(sizeof(hpx_thread_t *)*(1 + ntboxes_));
  th_largs   = hpx_alloc(sizeof(long)*(1 + ntboxes_));
  fut_local  = hpx_alloc(sizeof(hpx_future_t *)*(1 + ntboxes_));

  for (int i = 1; i <= nsboxes_; i++) {
    th_mpole[i] = th_expo[i] = NULL; 
    th_margs[i] = th_eargs[i] = (long) i; 
    fut_mpole[i] = fut_expo[i] =  NULL; 
  }

  for (int i = 1; i <= ntboxes_; i++) {
    th_local[i] = NULL;
    th_largs[i] = (long) i;
    fut_expo[i] = NULL;
  }

  struct timespec fmm_timer; 
  hpx_timer_init(); 
  hpx_get_time(&fmm_timer); 

  for (int i = nsboxes_; i >= 1; i--)
    hpx_thread_create(NULL, 0, ComputeMultipole, &th_margs[i], &fut_mpole[i], &th_mpole[i]);

  for (int i = nsboxes_; i >= 1; i--)
    hpx_thread_create(NULL, 0, ComputeExponential, &th_eargs[i], &fut_expo[i], &th_expo[i]);

  for (int i = 1; i <= ntboxes_; i++)
    hpx_thread_create(NULL, 0, ComputeLocal, &th_largs[i], &fut_local[i], &th_local[i]);

  for (int i = 1; i <= ntboxes_; i++)
    hpx_thread_wait(fut_local[i]);  

  printf("%-51s%30.5e\n\n", 
	 "\nGraph traversal time:", 
	 hpx_elapsed_us(fmm_timer)*1e-6); 

  hpx_free(th_mpole);
  hpx_free(th_margs);
  hpx_free(fut_mpole);

  hpx_free(th_expo);
  hpx_free(th_eargs);
  hpx_free(fut_expo);

  hpx_free(th_local);
  hpx_free(th_largs);
  hpx_free(fut_local);

  //hpx_cleanup(); 
}

void ComputeMultipole(void *data)
{
  long boxid = *((long *) data);
  Box *sbox = sboxptrs_[boxid]; 

  if (sbox->nchild) {
    for (int i = 0; i < 8; i++) {
      int child = sbox->child[i];
      if (child)
	hpx_thread_wait(fut_mpole[child]);
    }

    MultipoleToMultipole(sbox);
  } else {
    SourceToMultipole(sbox);
  }

  hpx_thread_exit(NULL);
}

void ComputeExponential(void *data)
{
  long boxid = *((long *) data);
  Box *sbox = sboxptrs_[boxid]; 

  hpx_thread_wait(fut_mpole[boxid]);

  MultipoleToExponential(sbox);

  hpx_thread_exit(NULL);
}

void ComputeLocal(void *data)
{
  long boxid = *((long *) data);
  Box *tbox = tboxptrs_[boxid]; 

  if (tbox->nchild) {
    int nlist5 = tbox->list5[0];
    int *list5 = &tbox->list5[1]; 
    for (int i = 0; i < nlist5; i++) {
      Box *sbox = sboxptrs_[list5[i]]; 
      for (int j = 0; j < 8; j++) {
	int child = sbox->child[j]; 
	if (child) 
	  hpx_thread_wait(fut_expo[child]);
      }
    }
    ExponentialToLocal(tbox);
  }

  int parent = tbox->parent; 
  if (parent) {
    hpx_thread_wait(fut_local[parent]);
    LocalToLocal(tbox);
  }

  ProcessList4(tbox);

  if (tbox->nchild == 0) {
    LocalToTarget(tbox);
    ProcessList13(tbox);
  }

  hpx_thread_exit(NULL);  
}

void SourceToMultipole(const Box *ibox)
{
  int addr = ibox->addr; 
  int level = ibox->level; 
  double complex *multipole = &mpole_[pgsz_*ibox->boxid]; 
  double *sources = &sources_[3*addr]; 
  double *charges = &charges_[addr]; 
  int nsources = ibox->npts; 
  double scale = scale_[level]; 
  double h = size_/pow(2, level + 1); 
  int ix = ibox->idx, iy = ibox->idy, iz = ibox->idz; 
  double x0 = corner_[0] + (2*ix + 1)*h; 
  double y0 = corner_[1] + (2*iy + 1)*h; 
  double z0 = corner_[2] + (2*iz + 1)*h; 

  const double precision = 1.0e-14;
  double *powers = calloc(pterms_ + 1, sizeof(double));
  double *p = calloc(pgsz_, sizeof(double));
  double complex *ephi = calloc(pterms_ + 1, sizeof(double complex));  

  for (int i = 0; i < nsources; i++) {
    double rx = sources[3*i] - x0; 
    double ry = sources[3*i + 1] - y0;
    double rz = sources[3*i + 2] - z0; 
    double proj = rx*rx + ry*ry; 
    double rr = proj + rz*rz;
    proj = sqrt(proj);
    double d = sqrt(rr);
    double ctheta = (d <= precision ? 1.0 : rz/d);
    ephi[0] = (proj <= precision*d ? 1.0 : rx/proj + _Complex_I*ry/proj);

    d *= scale; 
    powers[0] = 1.0;
    for (int ell = 1; ell <= pterms_; ell++) {
      powers[ell] = powers[ell - 1]*d;
      ephi[ell] = ephi[ell - 1]*ephi[0];
    }

    multipole[0] += charges[i]; 

    lgndr(pterms_, ctheta, p);
    for (int ell = 1; ell <= pterms_; ell++) {
      double cp = charges[i]*powers[ell]*p[ell]; 
      multipole[ell] += cp;
    }

    for (int m = 1; m <= pterms_; m++) {
      int offset = m*(pterms_ + 1);
      for (int ell = m; ell <= pterms_; ell++) {
	double cp = charges[i]*powers[ell]*ytopc_[ell + offset]*p[ell + offset];
	multipole[ell + offset] += cp*conj(ephi[m - 1]);
      }
    }
  }

  free(powers);
  free(ephi);
  free(p);
}

void MultipoleToMultipole(const Box *pbox)
{
  static const double complex var[5] = 
    {1,-1 + _Complex_I, 1 + _Complex_I, 1 - _Complex_I, -1 - _Complex_I};
  const double arg = sqrt(2)/2.0;

  double complex *pmultipole = &mpole_[pgsz_*pbox->boxid]; 
  int lev = pbox->level; 
  double sc1 = scale_[lev + 1], sc2 = scale_[lev]; 

 double *powers = calloc(pterms_ + 3,sizeof(double));
 double complex *mpolen = calloc(pgsz_, sizeof(double complex));
 double complex *marray = calloc(pgsz_, sizeof(double complex));
 double complex *ephi = calloc(pterms_ + 3, sizeof(double complex));

 for (int i = 0; i < 8; i++) {
   int child = pbox->child[i]; 
   if (child) {
     Box *cbox = sboxptrs_[child]; 
     int ifl = iflu_[i]; 
     double *rd = (i < 4? rdsq3_ : rdmsq3_);
     double complex *cmultipole = &mpole_[pgsz_*cbox->boxid]; 

     ephi[0] = 1.0; 
     ephi[1] = arg*var[ifl]; 
     double dd = -sqrt(3)/2.0; 
     powers[0] = 1.0; 

     for (int ell = 1; ell <= pterms_ + 1; ell++) {
       powers[ell] = powers[ell - 1]*dd; 
       ephi[ell + 1] = ephi[ell]*ephi[1]; 
     }

     for (int m = 0; m <= pterms_; m++) {
       int offset = m*(pterms_ + 1);
       for (int ell = m; ell <= pterms_; ell++) {
	 int index = ell + offset; 
	 mpolen[index] = conj(ephi[m])*cmultipole[index]; 
       }
     }

     for (int m = 0; m <= pterms_; m++) {
       int offset = m*(pterms_ + 1);
       int offset1 = (m + pterms_)*pgsz_; 
       int offset2 = (-m + pterms_)*pgsz_; 
       for (int ell = m; ell <= pterms_; ell++) {
	 int index = offset + ell; 
	 marray[index] = mpolen[ell]*rd[ell + offset1];
	 for (int mp = 1; mp <= ell; mp++) {
	   int index1 = ell + mp*(pterms_ + 1);
	       marray[index] += mpolen[index1]*rd[index1 + offset1] + 
		 conj(mpolen[index1])*rd[index1 + offset2];
	 }
       }
     }

     for (int k = 0; k <= pterms_; k++) {
       int offset = k*(pterms_ + 1);
       for (int j = k; j <= pterms_; j++) {
	 int index = offset + j;
	 mpolen[index] = marray[index]; 
	 for (int ell = 1; ell <= j - k; ell++) {
	   int index2 = j - k + ell*(2*pterms_ + 1);
	   int index3 = j + k + ell*(2*pterms_ + 1);
	       mpolen[index] += marray[index - ell]*powers[ell]*dc_[index2]*dc_[index3]; 	    
	 }
       }
     }

     for (int m = 0; m <= pterms_; m = m + 2) {
       int offset = m*(pterms_ + 1);
       int offset1 = (m + pterms_)*pgsz_;
       int offset2 = (-m + pterms_)*pgsz_;
       for (int ell = m; ell <= pterms_; ell++) {
	 int index = ell + offset;
	 marray[index] = mpolen[ell]*rd[ell + offset1];
	 for (int mp = 1; mp <= ell; mp = mp + 2) {
	   int index1 = ell + mp*(pterms_ + 1);
	       marray[index] -= mpolen[index1]*rd[index1 + offset1] + 
		 conj(mpolen[index1])*rd[index1 + offset2];
	 }

	 for (int mp = 2; mp <= ell; mp = mp + 2) {
	   int index1 = ell + mp*(pterms_ + 1);
	       marray[index] += mpolen[index1]*rd[index1 + offset1] + 
		 conj(mpolen[index1])*rd[index1 + offset2];
	 }
       }
     }

     for (int m = 1; m <= pterms_; m = m + 2) {
       int offset = m*(pterms_ + 1);
       int offset1 = (m + pterms_)*pgsz_;
       int offset2 = (-m + pterms_)*pgsz_;
       for (int ell = m; ell <= pterms_; ell++) {
	 int index = ell + offset;
	 marray[index] = -mpolen[ell]*rd[ell + offset1];
	 for (int mp = 1; mp <= ell; mp = mp + 2) {
	   int index1 = ell + mp*(pterms_ + 1);
	       marray[index] += mpolen[index1]*rd[index1 + offset1] + 
		 conj(mpolen[index1])*rd[index1 + offset2];
	 }

	 for (int mp = 2; mp <= ell; mp = mp + 2) {
	   int index1 = ell + mp*(pterms_ + 1);
	       marray[index] -= mpolen[index1]*rd[index1 + offset1] + 
		 conj(mpolen[index1])*rd[index1 + offset2];
	 }
       }
     }
      
     powers[0] = 1.0;
     dd = sc2/sc1;
     for (int ell = 1; ell <= pterms_ + 1; ell++) {
       powers[ell] = powers[ell-1]*dd;
     }
      
     for (int m = 0; m <= pterms_; m++) {
       int offset = m*(pterms_ + 1);
       for (int ell = m; ell <= pterms_; ell++) {
	 int index = ell + offset;
	 mpolen[index] = ephi[m]*marray[index]*powers[ell];
       }
     }
      
     for (int m = 0; m < pgsz_; m++) 
       pmultipole[m] += mpolen[m];
   }
 }
 free(ephi);
 free(powers);
 free(mpolen);
 free(marray);
}

void MultipoleToExponential(const Box *ibox)
{
  int boxid = ibox->boxid; 
  double complex *mw = calloc(pgsz_, sizeof(double complex));
  double complex *mexpf1 = calloc(nexpmax_, sizeof(double complex));
  double complex *mexpf2 = calloc(nexpmax_, sizeof(double complex));

  MultipoleToExponentialPhase1(&mpole_[pgsz_*boxid], mexpf1, mexpf2);
  MultipoleToExponentialPhase2(mexpf1, &expu_[nexptotp_*boxid]);
  MultipoleToExponentialPhase2(mexpf2, &expd_[nexptotp_*boxid]); 

  rotz2y(&mpole_[pgsz_*boxid], rdminus_, mw);
  MultipoleToExponentialPhase1(mw, mexpf1, mexpf2);
  MultipoleToExponentialPhase2(mexpf1, &expn_[nexptotp_*boxid]);
  MultipoleToExponentialPhase2(mexpf2, &exps_[nexptotp_*boxid]); 

  rotz2x(&mpole_[pgsz_*boxid], rdplus_, mw); 
  MultipoleToExponentialPhase1(mw, mexpf1, mexpf2);
  MultipoleToExponentialPhase2(mexpf1, &expe_[nexptotp_*boxid]);
  MultipoleToExponentialPhase2(mexpf2, &expw_[nexptotp_*boxid]);

  free(mw);
  free(mexpf1);
  free(mexpf2);
}

void MultipoleToExponentialPhase1(const double complex *multipole, 
				  double complex *mexpu, double complex *mexpd)
{
  int ntot = 0;
  for (int nell = 0; nell < nlambs_; nell++) {
    double sgn = -1.0;
    double complex zeyep = 1.0;
    for (int mth = 0; mth <= numfour_[nell] - 1; mth++) {
      int ncurrent = ntot + mth ;
      double complex ztmp1 = 0.0;
      double complex ztmp2 = 0.0;
      sgn = -sgn;
      int offset = mth*(pterms_ + 1);
      int offset1 = offset + nell*pgsz_;
      for (int nm = mth; nm <= pterms_; nm = nm + 2) {
	ztmp1 += rlsc_[nm + offset1]*multipole[nm + offset];
      }
      for (int nm = mth + 1; nm <= pterms_; nm = nm + 2) {
	ztmp2 += rlsc_[nm + offset1]*multipole[nm + offset];
      }

      mexpu[ncurrent] = (ztmp1 + ztmp2)*zeyep;
      mexpd[ncurrent] = sgn*(ztmp1 - ztmp2)*zeyep;
      zeyep = zeyep*_Complex_I;
    }
    ntot = ntot + numfour_[nell];
  }
}

void MultipoleToExponentialPhase2(const double complex *mexpf, double complex *mexpphys)
{
  int nftot = 0, nptot = 0, nexte = 0, nexto = 0;
  for (int i = 0; i < nlambs_; i++) {
    for (int ival = 0; ival < numphys_[i]/2; ival++) {
      mexpphys[nptot + ival] = mexpf[nftot];     
      for (int nm = 1; nm < numfour_[i]; nm = nm + 2) {
	double rt1 = cimag(fexpe_[nexte])*creal(mexpf[nftot + nm]);
	double rt2 = creal(fexpe_[nexte])*cimag(mexpf[nftot + nm]);
	double rtmp = 2*(rt1 + rt2);
	nexte++;
	mexpphys[nptot + ival] += rtmp*_Complex_I; 
      }
      
      for (int nm = 2; nm < numfour_[i]; nm = nm + 2) {
	double rt1 = creal(fexpo_[nexto])*creal(mexpf[nftot + nm]);
	double rt2 = cimag(fexpo_[nexto])*cimag(mexpf[nftot + nm]);
	double rtmp = 2*(rt1 - rt2);
	nexto++;
	mexpphys[nptot + ival] += rtmp;
      }      
    }    
    nftot += numfour_[i]; 
    nptot += numphys_[i]/2; 
  }
}

void ExponentialToLocal(const Box *ibox)
{
  int uall[36], nuall, xuall[36], yuall[36], 
    u1234[16], nu1234,x1234[16],y1234[16],
    dall[36], ndall, xdall[36], ydall[36], 
    d5678[16], nd5678, x5678[16], y5678[16], 
    nall[24], nnall, xnall[24], ynall[24], 
    n1256[8], nn1256, x1256[8], y1256[8], 
    n12[4], nn12, x12[4], y12[4], n56[4], nn56, x56[4], y56[4], 
    sall[24], nsall, xsall[24], ysall[24], 
    s3478[8], ns3478, x3478[8], y3478[8], 
    s34[4], ns34, x34[4], y34[4], s78[4], ns78, x78[4], y78[4], 
    eall[16], neall, xeall[16], yeall[16],
    e1357[4], ne1357, x1357[4], y1357[4], 
    e13[2], ne13, x13[2], y13[2], e57[2], ne57, x57[2], y57[2], 
    e1[3], ne1, x1[3], y1[3], e3[3], ne3, x3[3], y3[3], 
    e5[3], ne5, x5[3], y5[3], e7[3], ne7, x7[3], y7[3], 
    wall[16], nwall, xwall[16], ywall[16], 
    w2468[4], nw2468, x2468[4], y2468[4], 
    w24[2], nw24, x24[2], y24[2], w68[2], nw68, x68[2], y68[2], 
    w2[3], nw2, x2[3], y2[3], w4[3], nw4, x4[3], y4[3], 
    w6[3], nw6, x6[3], y6[3], w8[3], nw8, x8[3], y8[3];
  
  BuildMergedList2(ibox, uall, &nuall, xuall, yuall, 
		   u1234, &nu1234, x1234, y1234, 
		   dall, &ndall, xdall, ydall, 
		   d5678, &nd5678, x5678, y5678,
		   nall, &nnall, xnall, ynall, 
		   n1256, &nn1256, x1256, y1256, 
		   n12, &nn12, x12, y12, n56, &nn56, x56, y56, 
		   sall, &nsall, xsall, ysall,
		   s3478, &ns3478, x3478, y3478, 
		   s34, &ns34, x34, y34, s78, &ns78, x78, y78, 
		   eall, &neall, xeall, yeall, 
		   e1357, &ne1357, x1357, y1357, 
		   e13, &ne13, x13, y13, e57, &ne57, x57, y57, 
		   e1, &ne1, x1, y1, e3, &ne3, x3, y3, 
		   e5, &ne5, x5, y5, e7, &ne7, x7, y7, 
		   wall, &nwall, xwall, ywall, 
		   w2468, &nw2468, x2468, y2468, 
		   w24, &nw24, x24, y24, w68, &nw68, x68, y68, 
		   w2, &nw2, x2, y2, w4, &nw4, x4, y4, 
		   w6, &nw6, x6, y6, w8, &nw8, x8, y8);

  int ilev = ibox->level; 
  double scale = scale_[ilev + 1]; 
  double complex *mw1 = calloc(pgsz_, sizeof(double complex));
  double complex *mw2 = calloc(pgsz_, sizeof(double complex));
  double complex *temp = calloc(nexpmax_, sizeof(double complex));
  double complex *mexpf1 = calloc(nexpmax_, sizeof(double complex));
  double complex *mexpf2 = calloc(nexpmax_, sizeof(double complex));
  double complex *work = calloc(nexpmax_*16, sizeof(double complex)); 

  // Process z-direction exponential expansions and write results to uall, u1234, dall,
  // and d5678 lists. 
  double complex *mexuall = work; 
  double complex *mexu1234 = mexuall + nexpmax_; 
  double complex *mexdall = mexu1234 + nexpmax_; 
  double complex *mexd5678 = mexdall + nexpmax_; 

  MakeUList(nexptotp_, expd_, uall, nuall, xuall, yuall, xs_, ys_, mexuall); 
  MakeUList(nexptotp_, expd_, u1234, nu1234, x1234, y1234, xs_, ys_, mexu1234);
  MakeDList(nexptotp_, expu_, dall, ndall, xdall, ydall, xs_, ys_, mexdall);
  MakeDList(nexptotp_, expu_, d5678, nd5678, x5678, y5678, xs_, ys_, mexd5678);

  if (ibox->child[0]) {
    double complex *local = &local_[pgsz_*ibox->child[0]]; 
    int iexp1 = 0; 
    int iexp2 = 0; 
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0; 

    if (nuall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexuall[j]*zs_[3*j + 2]*scale; 
      iexp1++;
    }

    if (nu1234) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexu1234[j]*zs_[3*j + 1]*scale;
      iexp1++;
    }

    if (iexp1)
      ExponentialToLocalPhase1(temp, mexpf1);  
    
    if (ndall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexdall[j]*zs_[3*j + 1]*scale;
      iexp2++;
      ExponentialToLocalPhase1(temp, mexpf2); 
    }

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw1[j]; 
    }
  }

  if (ibox->child[1]) {
    double complex *local = &local_[pgsz_*ibox->child[1]];
    int iexp1 = 0;
    int iexp2 = 0;
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexuall[j]*zs_[3*j + 2]*conj(xs_[3*j])*scale;   
      iexp1++;
    }

    if (nu1234) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexu1234[j]*zs_[3*j + 1]*conj(xs_[3*j])*scale;   
      iexp1++;
    }

    if (iexp1) 
      ExponentialToLocalPhase1(temp, mexpf1);

    if (ndall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexdall[j]*zs_[3*j + 1]*xs_[3*j]*scale;
      ExponentialToLocalPhase1(temp, mexpf2);
      iexp2++;
    }
      
    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw1[j]; 
    }
  }

  if (ibox->child[2]) {
    double complex *local = &local_[pgsz_*ibox->child[2]];
    int iexp1 = 0;
    int iexp2 = 0;
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexuall[j]*zs_[3*j + 2]*conj(ys_[3*j])*scale;
      iexp1++;
    }

    if (nu1234) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexu1234[j]*zs_[3*j + 1]*conj(ys_[3*j])*scale;
      iexp1++;
    }

    if (iexp1) 
      ExponentialToLocalPhase1(temp, mexpf1);
    
    if (ndall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexdall[j]*zs_[3*j + 1]*ys_[3*j]*scale;
      iexp2++;
      ExponentialToLocalPhase1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw1[j]; 
    }
  }

  if (ibox->child[3]) {
    double complex *local = &local_[pgsz_*ibox->child[3]];
    int iexp1 = 0;
    int iexp2 = 0;
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexuall[j]*zs_[3*j + 2]*conj(xs_[3*j]*ys_[3*j])*scale;
      iexp1++;
    }
      
    if (nu1234) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexu1234[j]*zs_[3*j + 1]*conj(xs_[3*j]*ys_[3*j])*scale;
      iexp1++;
    }

    if (iexp1) 
      ExponentialToLocalPhase1(temp, mexpf1);
      
    if (ndall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexdall[j]*zs_[3*j + 1]*xs_[3*j]*ys_[3*j]*scale;
      iexp2++;
      ExponentialToLocalPhase1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw1[j];
    }
  }

  if (ibox->child[4]) {
    double complex *local = &local_[pgsz_*ibox->child[4]];
    int iexp1 = 0;
    int iexp2 = 0;      

    if (nuall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexuall[j]*zs_[3*j + 1]*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1); 
    }

    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (ndall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexdall[j]*zs_[3*j + 2]*scale;
      iexp2++;
    }
      
    if (nd5678) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexd5678[j]*zs_[3*j + 1]*scale;
      iexp2++;
    }
      
    if (iexp2) 
      ExponentialToLocalPhase1(temp, mexpf2);
      
    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw1[j]; 
    }
  }

  if (ibox->child[5]) {
    double complex *local = &local_[pgsz_*ibox->child[5]];
    int iexp1 = 0;
    int iexp2 = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexuall[j]*zs_[3*j + 1]*conj(xs_[3*j])*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1);
    }
      
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;
      
    if (ndall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexdall[j]*zs_[3*j + 2]*xs_[3*j]*scale;
      iexp2++;
    }
      
    if (nd5678) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexd5678[j]*zs_[3*j + 1]*xs_[3*j]*scale;
      iexp2++;
    }

    if (iexp2) 
      ExponentialToLocalPhase1(temp, mexpf2);

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw1[j]; 
    }
  }

  if (ibox->child[6]) {
    double complex *local = &local_[pgsz_*ibox->child[6]]; 
    int iexp1 = 0;
    int iexp2 = 0;      

    if (nuall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexuall[j]*zs_[3*j + 1]*conj(ys_[3*j])*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;
      
    if (ndall)  {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexdall[j]*zs_[3*j + 2]*ys_[3*j]*scale;
      iexp2++;
    }
      
    if (nd5678) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexd5678[j]*zs_[3*j + 1]*ys_[3*j]*scale;
      iexp2++;
    }
      
    if (iexp2) 
      ExponentialToLocalPhase1(temp, mexpf2);
      
    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw1[j]; 
    }
  }

  if (ibox->child[7]) {
    double complex *local = &local_[pgsz_*ibox->child[7]];
    int iexp1 = 0;
    int iexp2 = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexuall[j]*zs_[3*j + 1]*conj(xs_[3*j]*ys_[3*j])*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1);
    }
      
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (ndall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexdall[j]*zs_[3*j + 2]*xs_[3*j]*ys_[3*j]*scale;
      iexp2++;
    }
      
    if (nd5678) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexd5678[j]*zs_[3*j + 1]*xs_[3*j]*ys_[3*j]*scale;
      iexp2++;
    }
      
    if (iexp2) 
      ExponentialToLocalPhase1(temp, mexpf2);
      
    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw1[j]; 
    }
  }   

  // Process y-direction exponential expansions and write results to nall, n1256, n12,
  // n56, sall, s3478, s34, and s78 lists.  
  double complex *mexnall = work; 
  double complex *mexn1256 = mexnall + nexpmax_; 
  double complex *mexn12 = mexn1256 + nexpmax_; 
  double complex *mexn56 = mexn12 + nexpmax_; 
  double complex *mexsall = mexn56 + nexpmax_; 
  double complex *mexs3478 = mexsall + nexpmax_; 
  double complex *mexs34 = mexs3478 + nexpmax_; 
  double complex *mexs78 = mexs34 + nexpmax_; 

  MakeUList(nexptotp_, exps_, nall, nnall, xnall, ynall, xs_, ys_, mexnall); 
  MakeUList(nexptotp_, exps_, n1256, nn1256, x1256, y1256, xs_, ys_, mexn1256);
  MakeUList(nexptotp_, exps_, n12, nn12, x12, y12, xs_, ys_, mexn12); 
  MakeUList(nexptotp_, exps_, n56, nn56, x56, y56, xs_, ys_, mexn56);
  MakeDList(nexptotp_, expn_, sall, nsall, xsall, ysall, xs_, ys_, mexsall); 
  MakeDList(nexptotp_, expn_, s3478, ns3478, x3478, y3478, xs_, ys_, mexs3478); 
  MakeDList(nexptotp_, expn_, s34, ns34, x34, y34, xs_, ys_, mexs34); 
  MakeDList(nexptotp_, expn_, s78, ns78, x78, y78, xs_, ys_, mexs78); 

  if (ibox->child[0]) {
    double complex *local = &local_[pgsz_*ibox->child[0]];
    int iexp1 = 0;
    int iexp2 = 0;
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexnall[j]*zs_[3*j + 2]*scale;
      iexp1++;
    }
      
    if (nn1256) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexn1256[j]*zs_[3*j + 1]*scale;
      iexp1++;
    }
      
    if (nn12) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexn12[j]*zs_[3*j + 1]*scale;
      iexp1++;
    }
      
    if (iexp1) 
      ExponentialToLocalPhase1(temp, mexpf1);
      
    if (nsall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexsall[j]*zs_[3*j + 1]*scale;
      iexp2++;  
      ExponentialToLocalPhase1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j]; 
    }
  }
    
  if (ibox->child[1]) {
    double complex *local = &local_[pgsz_*ibox->child[1]];
    int iexp1 = 0;
    int iexp2 = 0;
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexnall[j]*zs_[3*j + 2]*conj(ys_[3*j])*scale;
      iexp1++;
    }
      
    if (nn1256) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexn1256[j]*zs_[3*j + 1]*conj(ys_[3*j])*scale;
      iexp1++;
    }

    if (nn12) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexn12[j]*zs_[3*j + 1]*conj(ys_[3*j])*scale;
      iexp1++;
    }

    if (iexp1) 
      ExponentialToLocalPhase1(temp, mexpf1);

    if (nsall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexsall[j]*zs_[3*j + 1]*ys_[3*j]*scale;
      iexp2++;
      ExponentialToLocalPhase1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j]; 
    }
  }

  if (ibox->child[2]) {
    double complex *local = &local_[pgsz_*ibox->child[2]];
    int iexp1 = 0; 
    int iexp2 = 0;
      
    if (nnall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexnall[j]*zs_[3*j + 1]*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1);
    }
      
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;
      
    if (nsall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexsall[j]*zs_[3*j + 2]*scale;
      iexp2++;
    }
      
    if (ns3478) {
      for (int j = 0; j < nexptotp_; j++)
	temp[j] += mexs3478[j]*zs_[3*j + 1]*scale;
      iexp2++;
    }

    if (ns34) {
      for (int j = 0; j < nexptotp_; j++)
	temp[j] += mexs34[j]*zs_[3*j + 1]*scale;
      iexp2++;
    }
    
    if (iexp2) 
      ExponentialToLocalPhase1(temp, mexpf2);
         
    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j]; 
    }
  }
    
  if (ibox->child[3]) {
    double complex *local = &local_[pgsz_*ibox->child[3]];
    int iexp1 = 0;
    int iexp2 = 0;
      
    if (nnall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexnall[j]*zs_[3*j + 1]*conj(ys_[3*j])*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1);
    }
      
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;
      
    if (nsall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexsall[j]*zs_[3*j + 2]*ys_[3*j]*scale;
      iexp2++;
    }

    if (ns3478) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexs3478[j]*zs_[3*j + 1]*ys_[3*j]*scale;
      iexp2++;
    }
      
    if (ns34) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexs34[j]*zs_[3*j + 1]*ys_[3*j]*scale;
      iexp2++;
    }

    if (iexp2)
      ExponentialToLocalPhase1(temp, mexpf2);
      
    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j];
    }
  }

  if (ibox->child[4]) {
    double complex *local = &local_[pgsz_*ibox->child[4]];
    int iexp1 = 0; 
    int iexp2 = 0;

    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;
      
    if (nnall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexnall[j]*zs_[3*j + 2]*conj(xs_[3*j])*scale;
      iexp1++;
    }

    if (nn1256) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexn1256[j]*zs_[3*j + 1]*conj(xs_[3*j])*scale;
      iexp1++;
    }

    if (nn56) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexn56[j]*zs_[3*j + 1]*conj(xs_[3*j])*scale;
      iexp1++;
    }
      
    if (iexp1) 
      ExponentialToLocalPhase1(temp, mexpf1);
      
    if (nsall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexsall[j]*zs_[3*j + 1]*xs_[3*j]*scale;
      ExponentialToLocalPhase1(temp, mexpf2);
      iexp2++;
    }
      
    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j];
    }
  }
    
  if (ibox->child[5]) {
    double complex *local = &local_[pgsz_*ibox->child[5]];
    int iexp1 = 0;
    int iexp2 = 0;

    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexnall[j]*zs_[3*j + 2]*conj(xs_[3*j]*ys_[3*j])*scale;
      iexp1++;
    }
      
    if (nn1256) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexn1256[j]*zs_[3*j + 1]*conj(xs_[3*j]*ys_[3*j])*scale;
      iexp1++;
    }
      
    if (nn56) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexn56[j]*zs_[3*j + 1]*conj(xs_[3*j]*ys_[3*j])*scale;
      iexp1++;
    }

    if (iexp1) 
      ExponentialToLocalPhase1(temp, mexpf1);
      
    if (nsall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexsall[j]*zs_[3*j + 1]*xs_[3*j]*ys_[3*j]*scale;
      iexp2++;
      ExponentialToLocalPhase1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j]; 
    }
  }

  if (ibox->child[6]) {
    double complex *local = &local_[pgsz_*ibox->child[6]];
    int iexp1 = 0;
    int iexp2 = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexnall[j]*zs_[3*j + 1]*conj(xs_[3*j])*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (nsall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexsall[j]*zs_[3*j + 2]*xs_[3*j]*scale;
      iexp2++;
    }

    if (ns3478) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexs3478[j]*zs_[3*j + 1]*xs_[3*j]*scale;
      iexp2++;
    }

    if (ns78) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexs78[j]*zs_[3*j + 1]*xs_[3*j]*scale;
      iexp2++;
    }

    if (iexp2) 
      ExponentialToLocalPhase1(temp, mexpf2);

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j]; 
    }
  } 

  if (ibox->child[7]) {
    double complex *local = &local_[pgsz_*ibox->child[7]];
    int iexp1 = 0; 
    int iexp2 = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexnall[j]*zs_[3*j + 1]*conj(xs_[3*j]*ys_[3*j])*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1);
    }
      
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (nsall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexsall[j]*zs_[3*j + 2]*xs_[3*j]*ys_[3*j]*scale;
      iexp2++;
    }

    if (ns3478) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexs3478[j]*zs_[3*j + 1]*xs_[3*j]*ys_[3*j]*scale;
      iexp2++;
    }
      
    if (ns78) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexs78[j]*zs_[3*j + 1]*xs_[3*j]*ys_[3*j]*scale;
      iexp2++;
    }

    if (iexp2) 
      ExponentialToLocalPhase1(temp, mexpf2);

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j]; 
    }
  }
    
  // Process x-direction exponential expansions and write results to eall, e1357, e13,
  // e57, e1, e3, e5, e7, wall, w2468, w24, w68, w2, w4, w6, and w8 lists. 
  double complex *mexeall = work;
  double complex *mexe1357 = mexeall + nexpmax_; 
  double complex *mexe13 = mexe1357 + nexpmax_; 
  double complex *mexe57 = mexe13 + nexpmax_; 
  double complex *mexe1 = mexe57 + nexpmax_; 
  double complex *mexe3 = mexe1 + nexpmax_; 
  double complex *mexe5 = mexe3 + nexpmax_; 
  double complex *mexe7 = mexe5 + nexpmax_; 
  double complex *mexwall = mexe7 + nexpmax_; 
  double complex *mexw2468 = mexwall + nexpmax_; 
  double complex *mexw24 = mexw2468 + nexpmax_; 
  double complex *mexw68 = mexw24 + nexpmax_; 
  double complex *mexw2 = mexw68 + nexpmax_; 
  double complex *mexw4 = mexw2 + nexpmax_; 
  double complex *mexw6 = mexw4 + nexpmax_; 
  double complex *mexw8 = mexw6 + nexpmax_; 

  MakeUList(nexptotp_, expw_, eall, neall, xeall, yeall, xs_, ys_, mexeall);
  MakeUList(nexptotp_, expw_, e1357, ne1357, x1357, y1357, xs_, ys_, mexe1357);
  MakeUList(nexptotp_, expw_, e13, ne13, x13, y13, xs_, ys_, mexe13);
  MakeUList(nexptotp_, expw_, e57, ne57, x57, y57, xs_, ys_, mexe57);
  MakeUList(nexptotp_, expw_, e1, ne1, x1, y1, xs_, ys_, mexe1); 
  MakeUList(nexptotp_, expw_, e3, ne3, x3, y3, xs_, ys_, mexe3);
  MakeUList(nexptotp_, expw_, e5, ne5, x5, y5, xs_, ys_, mexe5);
  MakeUList(nexptotp_, expw_, e7, ne7, x7, y7, xs_, ys_, mexe7);
  MakeDList(nexptotp_, expe_, wall, nwall, xwall, ywall, xs_, ys_, mexwall);
  MakeDList(nexptotp_, expe_, w2468, nw2468, x2468, y2468, xs_, ys_, mexw2468);
  MakeDList(nexptotp_, expe_, w24, nw24, x24, y24, xs_, ys_, mexw24);
  MakeDList(nexptotp_, expe_, w68, nw68, x68, y68, xs_, ys_, mexw68);
  MakeDList(nexptotp_, expe_, w2, nw2, x2, y2, xs_, ys_, mexw2);
  MakeDList(nexptotp_, expe_, w4, nw4, x4, y4, xs_, ys_, mexw4);
  MakeDList(nexptotp_, expe_, w6, nw6, x6, y6, xs_, ys_, mexw6);
  MakeDList(nexptotp_, expe_, w8, nw8, x8, y8, xs_, ys_, mexw8); 

  if (ibox->child[0]) {
    double complex *local = &local_[pgsz_*ibox->child[0]]; 
    int iexp1 = 0;
    int iexp2 = 0;
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (neall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexeall[j]*zs_[3*j + 2]*scale;
      iexp1++;
    }

    if (ne1357) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe1357[j]*zs_[3*j + 1]*scale;
      iexp1++;
    }

    if (ne13) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe13[j]*zs_[3*j + 1]*scale;
      iexp1++;
    }

    if (ne1) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe1[j]*zs_[3*j + 1]*scale;
      iexp1++;
    }

    if (iexp1) 
      ExponentialToLocalPhase1(temp, mexpf1);

    if (nwall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexwall[j]*zs_[3*j + 1]*scale;
      iexp2++;
      ExponentialToLocalPhase1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus_, mw2); 
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j];
    }
  }

  if (ibox->child[1]) {  
    double complex *local = &local_[pgsz_*ibox->child[1]];
    int iexp1 = 0;
    int iexp2 = 0;

    if (neall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexeall[j]*zs_[3*j + 1]*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (nwall) {      
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexwall[j]*zs_[3*j + 2]*scale;
      iexp2++;
    }
      
    if (nw2468) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw2468[j]*zs_[3*j + 1]*scale;
      iexp2++;
    }

    if (nw24) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw24[j]*zs_[3*j + 1]*scale;
      iexp2++;
    }

    if (nw2) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw2[j]*zs_[3*j + 1]*scale;
      iexp2++;
    }

    if (iexp2) 
      ExponentialToLocalPhase1(temp, mexpf2);

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus_, mw2); 
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j]; 
    }
  }

  if (ibox->child[2]) {
    double complex *local = &local_[pgsz_*ibox->child[2]]; 
    int iexp1 = 0;
    int iexp2 = 0;
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;
      
    if (neall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexeall[j]*zs_[3*j + 2]*conj(ys_[3*j])*scale;
      iexp1++;
    }
    
    if (ne1357) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe1357[j]*zs_[3*j + 1]*conj(ys_[3*j])*scale;
      iexp1++;
    }

    if (ne13) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe13[j]*zs_[3*j + 1]*conj(ys_[3*j])*scale;
      iexp1++;
    }

    if (ne3) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe3[j]*zs_[3*j + 1]*conj(ys_[3*j])*scale;
      iexp1++;
    }
      
    if (iexp1) 
      ExponentialToLocalPhase1(temp, mexpf1);

    if (nwall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexwall[j]*zs_[3*j + 1]*ys_[3*j]*scale;
      iexp2++;
      ExponentialToLocalPhase1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j];
    }
  }

  if (ibox->child[3]) {
    double complex *local = &local_[pgsz_*ibox->child[3]];
    int iexp1 = 0;
    int iexp2 = 0;
      
    if (neall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexeall[j]*zs_[3*j + 1]*conj(ys_[3*j])*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;
      
    if (nwall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexwall[j]*zs_[3*j + 2]*ys_[3*j]*scale;
      iexp2++;
    }

    if (nw2468) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw2468[j]*zs_[3*j + 1]*ys_[3*j]*scale;
      iexp2++;
    }

    if (nw24) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw24[j]*zs_[3*j + 1]*ys_[3*j]*scale;
      iexp2++;
    }

    if (nw4) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw4[j]*zs_[3*j + 1]*ys_[3*j]*scale;
      iexp2++;
    }

    if (iexp2) 
      ExponentialToLocalPhase1(temp, mexpf2);

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j]; 
    }
  }

  if (ibox->child[4]) {
    double complex *local = &local_[pgsz_*ibox->child[4]];
    int iexp1 = 0;
    int iexp2 = 0;

    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;
      
    if (neall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexeall[j]*zs_[3*j + 2]*xs_[3*j]*scale;
      iexp1++;
    }

    if (ne1357) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe1357[j]*zs_[3*j + 1]*xs_[3*j]*scale;
      iexp1++;
    }

    if (ne57) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe57[j]*zs_[3*j + 1]*xs_[3*j]*scale;
      iexp1++;
    }

    if (ne5) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe5[j]*zs_[3*j + 1]*xs_[3*j]*scale;
      iexp1++;
    }

    if (iexp1) 
      ExponentialToLocalPhase1(temp, mexpf1);

    if (nwall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexwall[j]*zs_[3*j + 1]*conj(xs_[3*j])*scale;
      iexp2++;
      ExponentialToLocalPhase1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j];
    }
  }

  if (ibox->child[5]) {
    double complex *local = &local_[pgsz_*ibox->child[5]];
    int iexp1 = 0;
    int iexp2 = 0;

    if (neall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexeall[j]*zs_[3*j + 1]*xs_[3*j]*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (nwall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexwall[j]*zs_[3*j + 2]*conj(xs_[3*j])*scale;
      iexp2++;
    }

    if (nw2468) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw2468[j]*zs_[3*j + 1]*conj(xs_[3*j])*scale;
      iexp2++;
    }

    if (nw68) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw68[j]*zs_[3*j + 1]*conj(xs_[3*j])*scale;
      iexp2++;
    }

    if (nw6) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw6[j]*zs_[3*j + 1]*conj(xs_[3*j])*scale;
      iexp2++;
    }

    if (iexp2) 
      ExponentialToLocalPhase1(temp, mexpf2);

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j]; 
    }
  }

  if (ibox->child[6]) {
    double complex *local = &local_[pgsz_*ibox->child[6]];
    int iexp1 = 0;
    int iexp2 = 0;
    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (neall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexeall[j]*zs_[3*j + 2]*xs_[3*j]*conj(ys_[3*j])*scale;
      iexp1++;
    }

    if (ne1357) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe1357[j]*zs_[3*j + 1]*xs_[3*j]*conj(ys_[3*j])*scale;
      iexp1++;
    }

    if (ne57) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe57[j]*zs_[3*j + 1]* xs_[3*j]*conj(ys_[3*j])*scale;
      iexp1++;
    }

    if (ne7) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexe7[j]*zs_[3*j + 1]*xs_[3*j]*conj(ys_[3*j])*scale;
      iexp1++;
    }

    if (iexp1) 
      ExponentialToLocalPhase1(temp, mexpf1);

    if (nwall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexwall[j]*zs_[3*j + 1]*conj(xs_[3*j])*ys_[3*j]*scale;
      iexp2++;
      ExponentialToLocalPhase1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j]; 
    }
  }

  if (ibox->child[7]) {
    double complex *local = &local_[pgsz_*ibox->child[7]];
    int iexp1 = 0;
    int iexp2 = 0;
      
    if (neall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexeall[j]*zs_[3*j + 1]*xs_[3*j]*conj(ys_[3*j])*scale;
      iexp1++;
      ExponentialToLocalPhase1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp_; j++) 
      temp[j] = 0;

    if (nwall) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] = mexwall[j]*zs_[3*j + 2]*conj(xs_[3*j])*ys_[3*j]*scale;
      iexp2++;
    }

    if (nw2468) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw2468[j]*zs_[3*j + 1]*conj(xs_[3*j])*ys_[3*j]*scale;
      iexp2++;
    }

    if (nw68) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw68[j]*zs_[3*j + 1]*conj(xs_[3*j])*ys_[3*j]*scale;
      iexp2++;
    }

    if (nw8) {
      for (int j = 0; j < nexptotp_; j++) 
	temp[j] += mexw8[j]*zs_[3*j+1]*conj(xs_[3*j])*ys_[3*j]*scale;
      iexp2++;
    }

    if (iexp2) 
      ExponentialToLocalPhase1(temp, mexpf2);
      
    if (iexp1 + iexp2) {
      ExponentialToLocalPhase2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus_, mw2);
      for (int j = 0; j < pgsz_; j++) 
	local[j] += mw2[j];
    }
  }

  free(mexpf1);
  free(mexpf2);
  free(temp);
  free(mw1);
  free(mw2);
  free(work);
}

void ExponentialToLocalPhase1(const double complex *mexpphys, double complex *mexpf)
{
  int nftot = 0, nptot = 0, next = 0; 
  for (int i = 0; i < nlambs_; i++) {
    int nalpha = numphys_[i]; 
    int nalpha2 = nalpha/2;
    mexpf[nftot] = 0;
    for (int ival = 0; ival < nalpha2; ival++) {
      mexpf[nftot] += 2.0*creal(mexpphys[nptot + ival]);
    }
    mexpf[nftot] /= nalpha;

    for (int nm = 2; nm < numfour_[i]; nm = nm + 2) {
      mexpf[nftot + nm] = 0;
      for (int ival = 0; ival < nalpha2; ival++) {
	double rtmp = 2*creal(mexpphys[nptot + ival]);
	mexpf[nftot + nm] += fexpback_[next]*rtmp;
	next += 1;
      }
      mexpf[nftot + nm] /= nalpha;
    }

    for (int nm = 1; nm < numfour_[i]; nm = nm + 2) {
      mexpf[nftot + nm] = 0;
      for (int ival = 0; ival < nalpha2; ival++) {
	double complex ztmp = 2*cimag(mexpphys[nptot + ival])*_Complex_I;
	mexpf[nftot + nm] += fexpback_[next]*ztmp;
	next += 1;
      }
      mexpf[nftot + nm] /= nalpha;
    }
    nftot += numfour_[i]; 
    nptot += numphys_[i]/2; 
  }
}

void ExponentialToLocalPhase2(int iexpu, const double complex *mexpu, 
			      int iexpd, const double complex * mexpd, 
			      double complex *local)
{
  double *rlampow = calloc(pterms_ + 1, sizeof(double));
  double complex *zeye = calloc(pterms_ + 1, sizeof(double complex));
  double complex *mexpplus = calloc(nexptot_, sizeof(double complex));
  double complex *mexpminus = calloc(nexptot_, sizeof(double complex));

  zeye[0] = 1.0;
  for (int i = 1; i <= pterms_; i++) {
    zeye[i] = zeye[i - 1]*_Complex_I;
  }

  for (int i = 0; i < pgsz_; i++) 
    local[i] = 0.0;

  for (int i = 0; i < nexptot_; i++) {
    if (iexpu <= 0) {
      mexpplus[i] = mexpd[i];
      mexpminus[i] = mexpd[i];
    } else if (iexpd <= 0) {
      mexpplus[i] = mexpu[i];
      mexpminus[i] = -mexpu[i];
    } else {
      mexpplus[i] = mexpd[i] + mexpu[i];
      mexpminus[i] = mexpd[i] - mexpu[i];
    }
  }

  int ntot = 0;
  for (int nell = 0; nell < nlambs_; nell++) {
    rlampow[0] = whts_[nell];
    double rmul = rlams_[nell];
    for (int j = 1; j <= pterms_; j++) 
      rlampow[j] = rlampow[j - 1]*rmul;    

    int mmax = numfour_[nell]-1;
    for (int mth = 0; mth <= mmax; mth = mth + 2) {
      int offset = mth*(pterms_ + 1);
      for (int nm = mth; nm <= pterms_; nm = nm + 2) {
	int index = offset + nm;
	int ncurrent = ntot + mth;
	rmul = rlampow[nm];
	local[index] += rmul*mexpplus[ncurrent];
      }

      for (int nm = mth + 1; nm <= pterms_; nm = nm + 2) {
	int index = offset + nm;
	int ncurrent = ntot + mth;
	rmul = rlampow[nm];
	local[index] += rmul*mexpminus[ncurrent];
      }
    }

    for (int mth = 1; mth <= mmax; mth = mth + 2) {
      int offset = mth*(pterms_ + 1);
      for (int nm = mth + 1; nm <= pterms_; nm = nm + 2) {
	int index = nm + offset;
	int ncurrent = ntot+mth;
	rmul = rlampow[nm];
	local[index] += rmul*mexpplus[ncurrent];
      }

      for (int nm = mth; nm <= pterms_; nm = nm + 2) {
	int index = nm + offset;
	int ncurrent = ntot + mth;
	rmul = rlampow[nm];
	local[index] += rmul*mexpminus[ncurrent];
      }
    }
    ntot += numfour_[nell];
  }

  for (int mth = 0; mth <= pterms_; mth++) {
    int offset = mth*(pterms_ + 1);
    for (int nm = mth; nm <= pterms_; nm++) {
      int index = nm + offset;
      local[index] *= zeye[mth]*ytopcs_[index];
    }
  }
  free(rlampow);
  free(zeye);
  free(mexpplus);
  free(mexpminus);
}

void LocalToLocal(const Box *ibox)
{
  static double complex var[5] = 
    {1, 1 - _Complex_I, -1 - _Complex_I, -1 + _Complex_I, 1 + _Complex_I};
  const double arg = sqrt(2)/2.0;

  double complex *localn = calloc(pgsz_, sizeof(double complex));
  double complex *marray = calloc(pgsz_, sizeof(double complex));
  double complex *ephi = calloc(1 + pterms_, sizeof(double complex)); 
  double *powers = calloc(1 + pterms_, sizeof(double)); 

  double sc1 = scale_[ibox->level - 1]; 
  double sc2 = scale_[ibox->level]; 
  int parent = ibox->parent; 
  Box *pbox = tboxptrs_[parent]; 
  double complex *plocal = &local_[pgsz_*parent]; 

  int ifl; 
  double *rd; 

  for (int i = 0; i < 8; i++) {
    if (ibox->boxid == pbox->child[i]) {
      ifl = ifld_[i]; 
      rd = (i < 4 ? rdsq3_ : rdmsq3_); 
      break; 
    }
  }

  ephi[0] = 1.0;
  ephi[1] = arg*var[ifl]; 
  double dd = -sqrt(3)/4.0;
  powers[0] = 1.0; 

  for (int ell = 1; ell <= pterms_; ell++)
    powers[ell] = powers[ell - 1]*dd; 

  for (int ell = 2; ell <= pterms_; ell++)
    ephi[ell] = ephi[ell - 1]*ephi[1]; 

  for (int m = 0; m <= pterms_; m++) {
    int offset = m*(pterms_ + 1);
    for (int ell = m; ell <= pterms_; ell++) {
      int index = ell + offset; 
      localn[index] = conj(ephi[m])*plocal[index];
    }
  }

  for (int m = 0; m <= pterms_; m++) {
    int offset = m*(pterms_ + 1);
    int offset1 = (pterms_ + m)*pgsz_; 
    int offset2 = (pterms_ - m)*pgsz_; 
    for (int ell = m; ell <= pterms_; ell++) {
      int index = ell + offset; 
      marray[index] = localn[ell]*rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp++) {
	int index1 = ell + mp*(pterms_ + 1);
	marray[index] += localn[index1]*rd[index1 + offset1] + 
	  conj(localn[index1])*rd[index1 + offset2];
      }
    }
  }

  for (int k = 0; k <= pterms_; k++) {
    int offset = k*(pterms_ + 1);
    for (int j = k; j <= pterms_; j++) {
      int index = j + offset; 
      localn[index] = marray[index]; 
      for (int ell = 1; ell <= pterms_ - j; ell++) {
	int index1 = ell + index; 
	int index2 = ell + j + k + ell*(2*pterms_ + 1);
	int index3 = ell + j - k + ell*(2*pterms_ + 1);
	localn[index] += marray[index1]*powers[ell]*dc_[index2]*dc_[index3];
      }
    }
  }

  for (int m = 0; m <= pterms_; m++) {
    int offset = m*(pterms_ + 1);
    int offset1 = (pterms_ + m)*pgsz_; 
    int offset2 = (pterms_ - m)*pgsz_; 
    for (int ell = m; ell <= pterms_; ell++) {
      int index = ell + offset; 
      marray[index] = localn[ell]*rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp = mp + 2) {
	int index1 = ell + mp*(pterms_ + 1);
	marray[index] -= localn[index1]*rd[index1 + offset1] + 
	  conj(localn[index1])*rd[index1 + offset2];
      }

      for (int mp = 2; mp <= ell; mp = mp + 2) {
	int index1 = ell + mp*(pterms_ + 1);
	marray[index] += localn[index1]*rd[index1 + offset1] + 
	  conj(localn[index1])*rd[index1 + offset2];
      }
    }
  }

  for (int m = 1; m <= pterms_; m = m + 2) {
    int offset = m*(pterms_ + 1);
    int offset1 = (pterms_ + m)*pgsz_; 
    int offset2 = (pterms_ - m)*pgsz_; 
    for (int ell = m; ell <= pterms_; ell++) {
      int index = ell + offset; 
      marray[index] = -localn[ell]*rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp = mp + 2) {
	int index1 = ell + mp*(pterms_ + 1);
	marray[index] += localn[index1]*rd[index1 + offset1] + 
	  conj(localn[index1])*rd[index1 + offset2];
      }

      for (int mp = 2; mp <= ell; mp = mp + 2) {
	int index1 = ell + mp*(pterms_ + 1);
	marray[index] -= localn[index1]*rd[index1 + offset1] + 
	  conj(localn[index1])*rd[index1 + offset2];
      }
    }
  }

  powers[0] = 1.0;
  dd = sc1/sc2;
  for (int ell = 1; ell <= pterms_; ell++)
    powers[ell] = powers[ell - 1]*dd; 

  for (int m = 0; m <= pterms_; m++) {
    int offset = m*(pterms_ + 1);
    for (int ell = m; ell <= pterms_; ell++) {
      int index = offset + ell;
      localn[index] = ephi[m]*marray[index]*powers[ell];
    }
  }

  double complex *local = &local_[pgsz_*ibox->boxid];
  for (int m = 0; m < pgsz_; m++) 
    local[m] += localn[m]; 

  free(ephi);
  free(powers);
  free(localn);
  free(marray);
}

void LocalToTarget(const Box *ibox)
{
  double complex *local = &local_[pgsz_*ibox->boxid];
  int level = ibox->level; 
  double scale = scale_[level]; 
  double h = size_/pow(2, level + 1);
  int ix = ibox->idx;
  int iy = ibox->idy;
  int iz = ibox->idz; 
  double x0 = corner_[0] + (2*ix + 1)*h;
  double y0 = corner_[1] + (2*iy + 1)*h;
  double z0 = corner_[2] + (2*iz + 1)*h;
  double *p = calloc(pgsz_, sizeof(double));
  double *powers = calloc(1 + pterms_, sizeof(double));
  double complex *ephi = calloc(1 + pterms_, sizeof(double complex));
  const double precision = 1.0e-14;

  for (int i = 0; i < ibox->npts; i++) {
    double field0, field1, field2, rloc, cp, rpotz = 0.0;
    double complex cpz, zs1 = 0.0, zs2 = 0.0, zs3 = 0.0;
    int ptr = ibox->addr + i;
    double *point = &targets_[3*ptr]; 
    double *pot = &potential_[ptr]; 
    double *field = &field_[3*ptr];  
    double rx = point[0] - x0;
    double ry = point[1] - y0;
    double rz = point[2] - z0;
    double proj  = rx*rx + ry*ry;
    double rr = proj + rz*rz;
    proj = sqrt(proj);
    double d = sqrt(rr);
    double ctheta = (d <= precision ? 0.0 : rz/d); 
    ephi[0] = (proj <= precision*d ? 1.0 : rx/proj + _Complex_I*ry/proj);
    d *= scale;
    double dd = d;

    powers[0] = 1.0;
    for (int ell = 1; ell <= pterms_; ell++) {
      powers[ell] = dd; 
      dd *= d; 
      ephi[ell] = ephi[ell - 1]*ephi[0]; 
    }

    lgndr(pterms_, ctheta, p); 
    *pot += creal(local[0]);
    
    field2 = 0.0;
    for (int ell = 1; ell <= pterms_; ell++) {
      rloc = creal(local[ell]);
      cp = rloc*powers[ell]*p[ell];
      *pot += cp;
      cp = powers[ell - 1]*p[ell - 1]*ytopcs_[ell - 1];
      cpz = local[ell + pterms_ + 1]*cp*ytopcsinv_[ell + pterms_ + 1];
      zs2 = zs2 + cpz;
      cp = rloc*cp*ytopcsinv_[ell];
      field2 += cp;
    }

    for (int ell = 1; ell <= pterms_; ell++) {
      for (int m = 1; m <= ell; m++) {
	int index = ell + m*(pterms_ + 1); 
	cpz = local[index]*ephi[m - 1];
	rpotz += creal(cpz)*powers[ell]*ytopc_[index]*p[index];
      }
    
      for (int m = 1; m <= ell - 1; m++) {
	int index1 = ell + m*(pterms_ + 1);
	int index2 = index1 - 1;
	zs3 += local[index1]*ephi[m - 1]*powers[ell - 1]*
	  ytopc_[index2]*p[index2]*ytopcs_[index2]*ytopcsinv_[index1];
      }

      for (int m = 2; m <= ell; m++) {
	int index1 = ell + m*(pterms_+1); 
	int index2 = ell - 1 + (m - 1)*(pterms_ + 1);
	zs2 += local[index1]*ephi[m - 2]*ytopcs_[index2]*
	  ytopcsinv_[index1]*powers[ell - 1]*ytopc_[index2]*p[index2];
      }

      for (int m = 0; m <= ell - 2; m++) {
	int index1 = ell + m*(pterms_ + 1); 
	int index2 = ell - 1 + (m + 1)*(pterms_ + 1);
	zs1 += local[index1]*ephi[m]*ytopcs_[index2]*
	  ytopcsinv_[index1]*powers[ell - 1]*ytopc_[index2]*p[index2];
      }
    }

    *pot += 2.0*rpotz;
    field0 = creal(zs2 - zs1);
    field1 = -cimag(zs2 + zs1);
    field2 += 2.0*creal(zs3);

    field[0] += field0*scale;
    field[1] += field1*scale;
    field[2] -= field2*scale;
  } 
  free(powers);
  free(ephi);
  free(p);
}

void ProcessList13(const Box *ibox)
{
  if (ibox->list3) {
    int nlist3 = ibox->list3[0];
    int *list3 = &ibox->list3[1];
    for (int j = 0; j < nlist3; j++) 
      DirectEvaluation(ibox, sboxptrs_[list3[j]]);
  }

  if (ibox->list1) {
    int nlist1 = ibox->list1[0];
    int *list1 = &ibox->list1[1];
    for (int j = 0; j < nlist1; j++)
      DirectEvaluation(ibox, sboxptrs_[list1[j]]);
  }
}

void ProcessList4(const Box *ibox)
{
  if (ibox->list4) {
    int nlist4 = ibox->list4[0];
    int *list4 = &ibox->list4[1];
    for (int j = 0; j < nlist4; j++) 
      DirectEvaluation(ibox, sboxptrs_[list4[j]]);
  }
}

void DirectEvaluation(const Box *tbox, const Box *sbox)
{
  int start1 = tbox->addr, num1 = tbox->npts, end1 = start1 + num1 - 1; 
  int start2 = sbox->addr, num2 = sbox->npts, end2 = start2 + num2 - 1;

  for (int i = start1; i <= end1; i++) {
    double pot = 0, fx = 0, fy = 0, fz = 0; 
    int i3 = i*3; 
    for (int j = start2; j <= end2; j++) {
      int j3 = j*3; 
      const double *t = &targets_[i3]; 
      const double *s = &sources_[j3]; 
      const double q = charges_[j]; 
      {
	double rx = t[0] - s[0]; 
	double ry = t[1] - s[1]; 
	double rz = t[2] - s[2]; 
	double rr = rx*rx + ry*ry + rz*rz; 
	double rdis = sqrt(rr); 

	if (rr) {
	  pot += q/rdis; 
	  double rmul = q/(rdis*rr); 
	  fx += rmul*rx; 
	  fy += rmul*ry; 
	  fz += rmul*rz; 
	}
      }
    }
    potential_[i] += pot; 
    field_[i3] += fx; 
    field_[i3 + 1] += fy; 
    field_[i3 + 2] += fz;
  }
}

void frmini(void)
{
  double *factorial = calloc(1 + 2*pterms_, sizeof(double));
  double d = 1.0;
  factorial[0] = d;
  for (int ell = 1; ell <= 2*pterms_; ell++) {
    d *= sqrt(ell);
    factorial[ell] = d;
  }

  ytopcs_[0] = 1.0;
  ytopcsinv_[0] = 1.0;
  for (int m = 0; m <= pterms_; m++) {
    int offset = m*(pterms_ + 1);
    for (int ell = m; ell <= pterms_; ell++) {
      ytopc_[ell + offset] = factorial[ell - m]/factorial[ell + m];
      ytopcsinv_[ell + offset] = factorial[ell - m]*factorial[ell + m];
      ytopcs_[ell + offset] = 1.0/ytopcsinv_[ell + offset];
    }
  }

  free(factorial);
}

void rotgen(void)
{
  bnlcft(dc_, 2*pterms_);
  double theta = acos(0); 
  fstrtn(pterms_, rdplus_, dc_, theta);

  theta = -theta;
  fstrtn(pterms_, rdminus_, dc_, theta);

  theta = acos(sqrt(3)/3);
  fstrtn(pterms_, rdsq3_, dc_, theta);

  theta = acos(-sqrt(3)/3);
  fstrtn(pterms_, rdmsq3_, dc_, theta);
}

void bnlcft(double *c, int p)
{
  for (int n = 0; n <= p; n++)
    c[n] = 1.0;

  for (int m = 1; m <= p; m++) {
    int offset = m*(p + 1);
    int offset1 = offset - p - 1;
    c[m + offset] = 1.0;
    for (int n = m + 1; n <= p; n++) 
      c[n + offset] = c[n - 1 + offset] + c[n - 1 + offset1];
  }

  for (int m = 1; m <= p; m++) {
    int offset = m*(p + 1);
    for (int n = m + 1; n <= p; n++) {
      c[n + offset] = sqrt(c[n + offset]);
    }
  }
}

void fstrtn(int p, double *d, const double *sqc, double theta)
{
  const double precision = 1.0e-19;
  const double ww = sqrt(2)/2;
  double ctheta = cos(theta);
  ctheta = (fabs(ctheta) <= precision ? 0.0 : ctheta);
  double stheta = sin(-theta);
  stheta = (fabs(stheta) <= precision ? 0.0 : stheta);
  double hsthta = ww*stheta;
  double cthtap = ww*(1.0+ctheta);
  double cthtan = -ww*(1.0-ctheta);

  int ij, im, imp; 
  d[p*pgsz_] = 1.0;

  for (ij = 1; ij <= p; ij++) {
    for (im = -ij; im <= -1; im++) {
      int index = ij + (im + p)*pgsz_; 
      d[index] = -sqc[ij - im + 2*(1 + 2*p)]*d[ij-1 + (im + 1 + p)*pgsz_];
      if (im > 1 - ij) 
	d[index] += sqc[ij + im + 2*(1 + 2*p)]*d[ij - 1 + (im - 1 + p)*pgsz_];
      d[index] *= hsthta;

      if (im > -ij) 
	d[index] += d[ij - 1 + (im + p)*pgsz_]*ctheta*
	  sqc[ij + im + 2*p + 1]*sqc[ij - im + 2*p + 1];      
      d[index] /= ij;
    }

    d[ij + p*pgsz_] = d[ij - 1 + p*pgsz_]*ctheta;

    if (ij > 1) 
      d[ij + p*pgsz_] += hsthta*sqc[ij + 2*(1 + 2*p)]*
	(d[ij - 1 + (-1 + p)*pgsz_] + d[ij - 1 + (1 + p)*pgsz_])/ij;
    
    for (im = 1; im <= ij; im++) {
      int index = ij + (im + p)*pgsz_; 
      d[index] = -sqc[ij + im + 2*(1 + 2*p)]*d[ij - 1 + (im - 1 + p)*pgsz_];
      if (im < ij-1) 
	d[index] += sqc[ij - im + 2*(1 + 2*p)]*d[ij - 1 + (im + 1 + p)*pgsz_];
      d[index] *= hsthta;

      if (im < ij) 
	d[index] += d[ij- 1 + (im + p)*pgsz_]*ctheta*
	  sqc[ij + im + 2*p + 1]*sqc[ij - im + 2*p + 1];      
      d[index] /= ij;
    }

    for (imp = 1; imp <= ij; imp++) {
      for (im = -ij; im <= -1; im++) {
	int index1 = ij + imp*(p + 1) + (im + p)*pgsz_; 
	int index2 = ij - 1 + (imp - 1)*(p + 1) + (im + p)*pgsz_; 
	d[index1] = d[index2 + pgsz_]*cthtan*sqc[ij - im + 2*(2*p + 1)]; 
	if (im > 1 - ij) 
	  d[index1] -= d[index2 - pgsz_]*cthtap*sqc[ij + im + 2*(2*p + 1)]; 

	if (im > -ij) 
	    d[index1] += d[index2]*stheta*sqc[ij + im + 2*p + 1]*
	      sqc[ij - im + 2*p + 1];
	d[index1] *= ww/sqc[ij + imp + 2*(2*p + 1)];
      }      

      int index3 = ij + imp*(p + 1) + p*pgsz_; 
      int index4 = ij - 1 + (imp - 1)*(p + 1) + p*pgsz_; 
      d[index3] = ij*stheta*d[index4];
      if (ij > 1) 
	d[index3] -= sqc[ij + 2*(2*p + 1)]*
	  (d[index4 - pgsz_]*cthtap + d[index4 + pgsz_]*cthtan);
      d[index3] *= ww/sqc[ij + imp + 2*(2*p + 1)]; 

      for (im = 1; im <= ij; im++) {
	int index5 = ij + imp*(p + 1) + (im + p)*pgsz_; 
	int index6 = ij - 1 + (imp - 1)*(p + 1) + (im + p)*pgsz_; 
	d[index5] = d[index6 - pgsz_]*cthtap*sqc[ij + im + 2*(2*p + 1)]; 
	if (im < ij - 1) 
	  d[index5] -= d[index6 + pgsz_]*cthtan*sqc[ij - im + 2*(2*p + 1)]; 

	if (im < ij) 
	    d[index5] += d[index6]*stheta*sqc[ij + im + 2*p + 1]*
	      sqc[ij - im + 2*p + 1];
	d[index5] *= ww/sqc[ij + imp + 2*(2*p + 1)];
      }
    }
  }
}

void vwts(void)
{
  if (nlambs_ == 9) {
    rlams_[0] = 0.99273996739714473469540223504736787e-01;
    rlams_[1] = 0.47725674637049431137114652301534079e+00;
    rlams_[2] = 0.10553366138218296388373573790886439e+01;
    rlams_[3] = 0.17675934335400844688024335482623428e+01;
    rlams_[4] = 0.25734262935147067530294862081063911e+01;
    rlams_[5] = 0.34482433920158257478760788217186928e+01;
    rlams_[6] = 0.43768098355472631055818055756390095e+01;
    rlams_[7] = 0.53489575720546005399569367000367492e+01;
    rlams_[8] = 0.63576578531337464283978988532908261e+01;
    whts_[0]  = 0.24776441819008371281185532097879332e+00;
    whts_[1]  = 0.49188566500464336872511239562300034e+00;
    whts_[2]  = 0.65378749137677805158830324216978624e+00;
    whts_[3]  = 0.76433038408784093054038066838984378e+00;
    whts_[4]  = 0.84376180565628111640563702167128213e+00;
    whts_[5]  = 0.90445883985098263213586733400006779e+00;
    whts_[6]  = 0.95378613136833456653818075210438110e+00;
    whts_[7]  = 0.99670261613218547047665651916759089e+00;
    whts_[8]  = 0.10429422730252668749528766056755558e+01;
  } else if (nlambs_ == 18) { 
    rlams_[0]  = 0.52788527661177607475107009804560221e-01;
    rlams_[1]  = 0.26949859838931256028615734976483509e+00;
    rlams_[2]  = 0.63220353174689392083962502510985360e+00;
    rlams_[3]  = 0.11130756427760852833586113774799742e+01;
    rlams_[4]  = 0.16893949614021379623807206371566281e+01;
    rlams_[5]  = 0.23437620046953044905535534780938178e+01;
    rlams_[6]  = 0.30626998290780611533534738555317745e+01;
    rlams_[7]  = 0.38356294126529686394633245072327554e+01;
    rlams_[8]  = 0.46542473432156272750148673367220908e+01;
    rlams_[9]  = 0.55120938659358147404532246582675725e+01;
    rlams_[10] = 0.64042126837727888499784967279992998e+01;
    rlams_[11] = 0.73268800190617540124549122992902994e+01;
    rlams_[12] = 0.82774009925823861522076185792684555e+01;
    rlams_[13] = 0.92539718060248947750778825138695538e+01;
    rlams_[14] = 0.10255602723746401139237605093512684e+02;
    rlams_[15] = 0.11282088297877740146191172243561596e+02;
    rlams_[16] = 0.12334067909676926788620221486780792e+02;
    rlams_[17] = 0.13414920240172401477707353478763252e+02;
    whts_[0]   = 0.13438265914335215112096477696468355e+00;
    whts_[1]   = 0.29457752727395436487256574764614925e+00;
    whts_[2]   = 0.42607819361148618897416895379137713e+00;
    whts_[3]   = 0.53189220776549905878027857397682965e+00;
    whts_[4]   = 0.61787306245538586857435348065337166e+00;
    whts_[5]   = 0.68863156078905074508611505734734237e+00;
    whts_[6]   = 0.74749099381426187260757387775811367e+00;
    whts_[7]   = 0.79699192718599998208617307682288811e+00;
    whts_[8]   = 0.83917454386997591964103548889397644e+00;
    whts_[9]   = 0.87570092283745315508980411323136650e+00;
    whts_[10]  = 0.90792943590067498593754180546966381e+00;
    whts_[11]  = 0.93698393742461816291466902839601971e+00;
    whts_[12]  = 0.96382546688788062194674921556725167e+00;
    whts_[13]  = 0.98932985769673820186653756536543369e+00;
    whts_[14]  = 0.10143828459791703888726033255807124e+01;
    whts_[15]  = 0.10400365437416452252250564924906939e+01;
    whts_[16]  = 0.10681548926956736522697610780596733e+01;
    whts_[17]  = 0.11090758097553685690428437737864442e+01;
  }
}

void numthetahalf(void)
{
  if (nlambs_ == 9) {
    numfour_[0] = 2;
    numfour_[1] = 4;
    numfour_[2] = 4;
    numfour_[3] = 6;
    numfour_[4] = 6;
    numfour_[5] = 4;
    numfour_[6] = 6;
    numfour_[7] = 4;
    numfour_[8] = 2;
  } else if (nlambs_ == 18) {
    numfour_[0]  = 4;
    numfour_[1]  = 6;
    numfour_[2]  = 6;
    numfour_[3]  = 8;
    numfour_[4]  = 8;
    numfour_[5]  = 8;
    numfour_[6]  = 10;
    numfour_[7]  = 10;
    numfour_[8]  = 10;
    numfour_[9]  = 10;
    numfour_[10] = 12;
    numfour_[11] = 12;
    numfour_[12] = 12;
    numfour_[13] = 12;
    numfour_[14] = 12;
    numfour_[15] = 12;
    numfour_[16] = 8;
    numfour_[17] = 2;
  }
}

void numthetafour(void)
{
  if (nlambs_ == 9) {
    numphys_[0] = 4;
    numphys_[1] = 8;
    numphys_[2] = 12;
    numphys_[3] = 16;
    numphys_[4] = 20;
    numphys_[5] = 20;
    numphys_[6] = 24;
    numphys_[7] = 8;
    numphys_[8] = 2;
  } else if (nlambs_ == 18) {
    numphys_[0]  = 6;
    numphys_[1]  = 8;
    numphys_[2]  = 12;
    numphys_[3]  = 16;
    numphys_[4]  = 20;
    numphys_[5]  = 26;
    numphys_[6]  = 30;
    numphys_[7]  = 34;
    numphys_[8]  = 38;
    numphys_[9]  = 44;
    numphys_[10] = 48;
    numphys_[11] = 52;
    numphys_[12] = 56;
    numphys_[13] = 60;
    numphys_[14] = 60;
    numphys_[15] = 52;
    numphys_[16] = 4;
    numphys_[17] = 2;
  }
}

void rlscini(void)
{
  double *factorial = calloc(2*pterms_ + 1, sizeof(double));
  double *rlampow = calloc(pterms_ + 1, sizeof(double));

  factorial[0] = 1;
  for (int i = 1; i <= 2*pterms_; i++)
    factorial[i] = factorial[i-1]*sqrt(i);
 
  for (int nell = 0; nell < nlambs_; nell++) {
    double rmul = rlams_[nell];
    rlampow[0] = 1;
    for (int j = 1;  j <= pterms_; j++)
      rlampow[j] = rlampow[j - 1]*rmul;      
    for (int j = 0; j <= pterms_; j++) {
      for (int k = 0; k <= j; k++) {
	rlsc_[j + k*(pterms_ + 1) + nell*pgsz_] = rlampow[j]/factorial[j - k]/factorial[j + k];
      }
    }    
  }
    
  free(factorial);
  free(rlampow);
}

void mkfexp(void)
{
  int nexte = 0; 
  int nexto = 0; 
  double m_pi = acos(-1); 

  for (int i = 0; i < nlambs_; i++) {
    int nalpha = numphys_[i]; 
    int nalpha2 = nalpha/2; 
    double halpha = 2.0*m_pi/nalpha; 
    for (int j = 1; j <= nalpha2; j++) {
      double alpha = (j - 1)*halpha; 
      for (int nm = 2; nm <= numfour_[i]; nm = nm + 2) {
	fexpe_[nexte] = cexp((nm - 1)*_Complex_I*alpha);
	nexte++;
      }

      for (int nm = 3; nm <= numfour_[i]; nm = nm + 2) {
	fexpo_[nexto] = cexp((nm - 1)*_Complex_I*alpha);
	nexto++;
      }
    }
  }

  int next = 0; 
  for (int i = 0; i < nlambs_; i++) {
    int nalpha = numphys_[i]; 
    int nalpha2 = nalpha/2; 
    double halpha = 2.0*m_pi/nalpha; 
    for (int nm = 3; nm <= numfour_[i]; nm = nm + 2) {
      for (int j = 1; j <= nalpha2; j++) {
	double alpha = (j - 1)*halpha; 
	fexpback_[next] = cexp(-(nm - 1)*_Complex_I*alpha);
	next++;
      }
    }

    for (int nm = 2; nm <= numfour_[i]; nm = nm + 2) {
      for (int j = 1; j <= nalpha2; j++) {
	double alpha = (j - 1)*halpha; 
	fexpback_[next] = cexp(-(nm - 1)*_Complex_I*alpha);
	next++;
      }
    }
  }
}

void mkexps(void)
{
  int ntot = 0; 
  double m_pi = acos(-1); 
  for (int nell = 0; nell < nlambs_; nell++) {
    double hu = 2.0*m_pi/numphys_[nell]; 
    for (int mth = 0; mth < numphys_[nell]/2; mth++) {
      double u = mth*hu; 
      int ncurrent = 3*(ntot + mth);
      zs_[ncurrent]     = exp(-rlams_[nell]);
      zs_[ncurrent + 1] = zs_[ncurrent]*zs_[ncurrent]; 
      zs_[ncurrent + 2] = zs_[ncurrent]*zs_[ncurrent + 1];
      xs_[ncurrent]     = cexp(_Complex_I*cos(u)*rlams_[nell]);
      xs_[ncurrent + 1] = xs_[ncurrent]*xs_[ncurrent];
      xs_[ncurrent + 2] = xs_[ncurrent + 1]*xs_[ncurrent]; 
      ys_[ncurrent]     = cexp(_Complex_I*sin(u)*rlams_[nell]);
      ys_[ncurrent + 1] = ys_[ncurrent]*ys_[ncurrent]; 
      ys_[ncurrent + 2] = ys_[ncurrent + 1]*ys_[ncurrent]; 
    }
    ntot += numphys_[nell]/2; 
  }
}

void lgndr(int nmax, double x, double *y)
{
  int n;
  n = (nmax + 1)*(nmax + 1);
  for (int m = 0; m < n; m++) 
    y[m] = 0.0;

  double u = -sqrt(1 - x*x);
  y[0] = 1;

  y[1] = x*y[0]; 
  for (int n = 2; n <= nmax; n++) 
    y[n] = ((2*n - 1)*x*y[n - 1] - (n - 1)*y[n - 2])/n;

  int offset1 = nmax + 2;
  for (int m = 1; m <= nmax - 1; m++) {
    int offset2 = m*offset1;
    y[offset2] = y[offset2 - offset1]*u*(2*m - 1);
    y[offset2 + 1] = y[offset2]*x*(2*m + 1);
    for (int n = m + 2; n <= nmax; n++) {
      int offset3 = n + m*(nmax + 1);
      y[offset3] = ((2*n - 1)*x*y[offset3 - 1] - (n + m - 1)*y[offset3 - 2])/(n - m);
    }
  }

  y[nmax + nmax*(nmax + 1)] = y[nmax - 1 + (nmax - 1)*(nmax + 1)]*u*(2*nmax - 1);
}

void MakeUList(int nexpo, const double complex *expo, const int *list, int nlist, 
	       const int *xoff, const int *yoff, const double complex *xs,
	       const double complex *ys, double complex *mexpo)
{
  for (int i = 0; i < nexpo; i++) 
    mexpo[i] = 0; 

  if (nlist) {
    for (int i = 0; i < nlist; i++) {
      int offset = list[i]*nexpo; 
      for (int j = 0; j < nexpo; j++) {
	double complex zmul = 1; 
	if (xoff[i] > 0) 
	  zmul *= xs[3*j + xoff[i] - 1];
	if (xoff[i] < 0) 
	  zmul *= conj(xs[3*j - xoff[i] - 1]);
	if (yoff[i] > 0) 
	  zmul *= ys[3*j + yoff[i] - 1];
	if (yoff[i] < 0) 
	  zmul *= conj(ys[3*j - yoff[i] - 1]);
	mexpo[j] += zmul*expo[offset + j];
      }
    }
  }
}

void MakeDList(int nexpo, const double complex *expo, const int *list, int nlist, 
	       const int *xoff, const int *yoff, const double complex *xs, 
	       const double complex *ys, double complex *mexpo)
{
  for (int i = 0; i < nexpo; i++) 
    mexpo[i] = 0; 

  if (nlist) {
    for (int i = 0; i < nlist; i++) {
      int offset = list[i]*nexpo; 
      for (int j = 0; j < nexpo; j++) {
	double complex zmul = 1; 
	if (xoff[i] > 0) 
	  zmul *= conj(xs[3*j + xoff[i] - 1]);
	if (xoff[i] < 0) 
	  zmul *= xs[3*j - xoff[i] - 1];
	if (yoff[i] > 0) 
	  zmul *= conj(ys[3*j + yoff[i] - 1]);
	if (yoff[i] < 0) 
	  zmul *= ys[3*j - yoff[i] - 1];
	mexpo[j] += zmul*expo[offset + j];
      }
    }
  }
}

void rotz2y(const double complex *multipole, const double *rd, double complex *mrotate)
{
  double complex *mwork = calloc(pgsz_, sizeof(double complex));
  double complex *ephi = calloc(pterms_ + 1, sizeof(double complex));

  ephi[0] = 1.0;
  for (int m =1; m <= pterms_; m++) 
    ephi[m] = -ephi[m - 1]*_Complex_I;
 
  for (int m = 0; m <= pterms_; m++) {
    int offset = m*(pterms_ + 1);
    for (int ell = m; ell <= pterms_; ell++) {
      int index = offset + ell;
      mwork[index] = ephi[m]*multipole[index];
    }
  }

  for (int m = 0; m <= pterms_; m++) {
    int offset = m*(pterms_ + 1);
    for (int ell = m; ell <= pterms_; ell++) {
      int index = ell + offset;
      mrotate[index] = mwork[ell]*rd[ell + (m + pterms_)*pgsz_]; 
      for (int mp = 1; mp <= ell; mp++) {
	int index1 = ell + mp*(pterms_ + 1);
	mrotate[index] += 
	  mwork[index1]*rd[ell + mp*(pterms_ + 1) + (m + pterms_)*pgsz_] +
	  conj(mwork[index1])*rd[ell + mp*(pterms_ + 1) + (-m + pterms_)*pgsz_];
      }
    }
  }
  free(ephi);
  free(mwork);
}

void roty2z(const double complex *multipole, const double *rd, double complex *mrotate)
{
  double complex *mwork = calloc(pgsz_, sizeof(double complex));
  double complex *ephi = calloc(1 + pterms_, sizeof(double complex));

  ephi[0] = 1.0;
  for (int m = 1; m <= pterms_; m++) 
    ephi[m] = ephi[m - 1]*_Complex_I;
  
  for (int m = 0; m <= pterms_; m++) {
    int offset = m*(pterms_ + 1);
    for (int ell = m; ell <= pterms_; ell++) {
      int index = ell + offset;
      mwork[index] = multipole[ell]*rd[ell + (m + pterms_)*pgsz_];
      for (int mp = 1; mp <= ell; mp++) {
	int index1 = ell + mp*(pterms_ + 1);
	mwork[index] += 
	  multipole[index1]*rd[ell + mp*(pterms_ + 1) + (m + pterms_)*pgsz_] +
	  conj(multipole[index1] )*rd[ell + mp*(pterms_ + 1) + (pterms_ - m)*pgsz_];
      }
    }
  }
 
  for (int m = 0; m <= pterms_; m++) {
    int offset = m*(pterms_ + 1);
    for (int ell = m; ell <= pterms_; ell++) {
      int index = ell + offset;
      mrotate[index] = ephi[m]*mwork[index];
    }
  }

  free(ephi);
  free(mwork);
}

void rotz2x(const double complex *multipole, const double *rd, double complex *mrotate)
{
  int offset1 = pterms_*pgsz_; 
  for (int m = 0; m <= pterms_; m++) {
    int offset2 = m*(pterms_ + 1);
    int offset3 = m*pgsz_ + offset1;
    int offset4 = -m*pgsz_ + offset1; 
    for (int ell = m; ell <= pterms_; ell++) {
      mrotate[ell + offset2] = multipole[ell]*rd[ell + offset3];
      for (int mp = 1; mp <= ell; mp++) {
	int offset5 = mp*(pterms_ + 1);
	mrotate[ell + offset2] += multipole[ell + offset5]*rd[ell + offset3 + offset5] + 
	  conj(multipole[ell + offset5])*rd[ell + offset4 + offset5];
      }
    }
  }
}
