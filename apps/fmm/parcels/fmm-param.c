#include <stdlib.h>
#include <math.h>
#include "fmm-param.h"

fmm_param_t construct_param(int accuracy) {
  fmm_param_t fmm_param; 

  if (accuracy == 3) {
    fmm_param.pterms = 9;
    fmm_param.nlambs = 9; 
    fmm_param.pgsz   = 100;
  } else if (accuracy == 6) {
    fmm_param.pterms = 18;
    fmm_param.nlambs = 18;
    fmm_param.pgsz   = 361;
  }

  fmm_param.numphys = calloc(fmm_param.nlambs, sizeof(int)); 
  fmm_param.numfour = calloc(fmm_param.nlambs, sizeof(int)); 
  fmm_param.whts    = calloc(fmm_param.nlambs, sizeof(double)); 
  fmm_param.rlams   = calloc(fmm_param.nlambs, sizeof(double)); 

  int allocation_size = fmm_param.pgsz*(2*fmm_param.pterms + 1); 
  fmm_param.rdplus  = calloc(allocation_size, sizeof(double)); 
  fmm_param.rdminus = calloc(allocation_size, sizeof(double)); 
  fmm_param.rdsq3   = calloc(allocation_size, sizeof(double)); 
  fmm_param.rdmsq3  = calloc(allocation_size, sizeof(double)); 
  
  allocation_size = (2*fmm_param.pterms + 1)*(2*fmm_param.pterms + 1)*
    (2*fmm_param.pterms + 1); 
  fmm_param.dc      = calloc(allocation_size, sizeof(double)); 

  fmm_param.ytopc     = calloc(fmm_param.pgsz, sizeof(double)); 
  fmm_param.ytopcs    = calloc(fmm_param.pgsz, sizeof(double)); 
  fmm_param.ytopcsinv = calloc(fmm_param.pgsz, sizeof(double)); 

  fmm_param.rlsc = calloc(fmm_param.pgsz*fmm_param.nlambs, sizeof(double)); 

  frmini(&fmm_param); 
  rotgen(&fmm_param); 
  vwts(&fmm_param); 
  numthetahalf(&fmm_param); 
  numthetafour(&fmm_param); 
  rlscini(&fmm_param); 

  fmm_param.nexptot = 0; 
  fmm_param.nthmax = 0; 
  fmm_param.nexptotp = 0; 
  for (int i = 1; i <= fmm_param.nlambs; i++) {
    fmm_param.nexptot += fmm_param.numfour[i - 1]; 
    if (fmm_param.numfour[i - 1] > fmm_param.nthmax) 
      fmm_param.nthmax = fmm_param.numfour[i - 1]; 
    fmm_param.nexptotp += fmm_param.numphys[i - 1]; 
  }
  fmm_param.nexptotp /= 2.0; 
  fmm_param.nexpmax = (fmm_param.nexptot > fmm_param.nexptotp ? 
		       fmm_param.nexptot : 
		       fmm_param.nexptotp) + 1;

  allocation_size = fmm_param.nexpmax*3; 
  fmm_param.xs = calloc(allocation_size, sizeof(double complex)); 
  fmm_param.ys = calloc(allocation_size, sizeof(double complex)); 
  fmm_param.zs = calloc(allocation_size, sizeof(double)); 

  fmm_param.fexpe    = calloc(15000, sizeof(double complex)); 
  fmm_param.fexpo    = calloc(15000, sizeof(double complex)); 
  fmm_param.fexpback = calloc(15000, sizeof(double complex)); 

  mkfexp(&fmm_param); 
  mkexps(&fmm_param); 
  
  return fmm_param;
}


void destruct_param(fmm_param_t *fmm_param) {
  free(fmm_param->xs); 
  free(fmm_param->ys); 
  free(fmm_param->zs);
  free(fmm_param->fexpe);
  free(fmm_param->fexpo);
  free(fmm_param->fexpback);
  free(fmm_param->numphys);
  free(fmm_param->numfour);
  free(fmm_param->whts); 
  free(fmm_param->rlams);
  free(fmm_param->rdplus);
  free(fmm_param->rdminus);
  free(fmm_param->rdsq3); 
  free(fmm_param->rdmsq3); 
  free(fmm_param->dc); 
  free(fmm_param->ytopc);
  free(fmm_param->ytopcs);
  free(fmm_param->ytopcsinv);
  free(fmm_param->rlsc); 
}


void frmini(fmm_param_t *fmm_param) {
  double *ytopc     = fmm_param->ytopc; 
  double *ytopcs    = fmm_param->ytopcs; 
  double *ytopcsinv = fmm_param->ytopcsinv; 
  int pterms        = fmm_param->pterms; 

  double *factorial = calloc(1 + 2*pterms, sizeof(double));
  double d = 1.0;
  factorial[0] = d;
  for (int ell = 1; ell <= 2*pterms; ell++) {
    d *= sqrt(ell);
    factorial[ell] = d;
  }

  ytopcs[0] = 1.0;
  ytopcsinv[0] = 1.0;
  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      ytopc[ell + offset] = factorial[ell - m]/factorial[ell + m];
      ytopcsinv[ell + offset] = factorial[ell - m]*factorial[ell + m];
      ytopcs[ell + offset] = 1.0/ytopcsinv[ell + offset];
    }
  }

  free(factorial);
}


void rotgen(fmm_param_t *fmm_param) {
  double *dc      = fmm_param->dc; 
  double *rdplus  = fmm_param->rdplus; 
  double *rdminus = fmm_param->rdminus; 
  double *rdsq3   = fmm_param->rdsq3; 
  double *rdmsq3  = fmm_param->rdmsq3; 
  int pterms      = fmm_param->pterms; 
  int pgsz        = fmm_param->pgsz; 

  bnlcft(dc, 2*pterms); 

  double theta = acos(0); 
  fstrtn(pterms, rdplus, dc, theta, pgsz); 
  
  theta = -theta; 
  fstrtn(pterms, rdminus, dc, theta, pgsz); 

  theta = acos(sqrt(3)/3); 
  fstrtn(pterms, rdsq3, dc, theta, pgsz); 

  theta = acos(-sqrt(3)/3); 
  fstrtn(pterms, rdmsq3, dc, theta, pgsz); 
}


void bnlcft(double *c, int p) {
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


void fstrtn(int p, double *d, const double *sqc, double theta, int pgsz) {
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
  d[p*pgsz] = 1.0;

  for (ij = 1; ij <= p; ij++) {
    for (im = -ij; im <= -1; im++) {
      int index = ij + (im + p)*pgsz; 
      d[index] = -sqc[ij - im + 2*(1 + 2*p)]*d[ij-1 + (im + 1 + p)*pgsz];
      if (im > 1 - ij) 
	d[index] += sqc[ij + im + 2*(1 + 2*p)]*d[ij - 1 + (im - 1 + p)*pgsz];
      d[index] *= hsthta;

      if (im > -ij) 
	d[index] += d[ij - 1 + (im + p)*pgsz]*ctheta*
	  sqc[ij + im + 2*p + 1]*sqc[ij - im + 2*p + 1];      
      d[index] /= ij;
    }

    d[ij + p*pgsz] = d[ij - 1 + p*pgsz]*ctheta;

    if (ij > 1) 
      d[ij + p*pgsz] += hsthta*sqc[ij + 2*(1 + 2*p)]*
	(d[ij - 1 + (-1 + p)*pgsz] + d[ij - 1 + (1 + p)*pgsz])/ij;
    
    for (im = 1; im <= ij; im++) {
      int index = ij + (im + p)*pgsz; 
      d[index] = -sqc[ij + im + 2*(1 + 2*p)]*d[ij - 1 + (im - 1 + p)*pgsz];
      if (im < ij-1) 
	d[index] += sqc[ij - im + 2*(1 + 2*p)]*d[ij - 1 + (im + 1 + p)*pgsz];
      d[index] *= hsthta;

      if (im < ij) 
	d[index] += d[ij- 1 + (im + p)*pgsz]*ctheta*
	  sqc[ij + im + 2*p + 1]*sqc[ij - im + 2*p + 1];      
      d[index] /= ij;
    }

    for (imp = 1; imp <= ij; imp++) {
      for (im = -ij; im <= -1; im++) {
	int index1 = ij + imp*(p + 1) + (im + p)*pgsz; 
	int index2 = ij - 1 + (imp - 1)*(p + 1) + (im + p)*pgsz; 
	d[index1] = d[index2 + pgsz]*cthtan*sqc[ij - im + 2*(2*p + 1)]; 
	if (im > 1 - ij) 
	  d[index1] -= d[index2 - pgsz]*cthtap*sqc[ij + im + 2*(2*p + 1)]; 

	if (im > -ij) 
	      d[index1] += d[index2]*stheta*sqc[ij + im + 2*p + 1]*
		sqc[ij - im + 2*p + 1];
	d[index1] *= ww/sqc[ij + imp + 2*(2*p + 1)];
      }      

      int index3 = ij + imp*(p + 1) + p*pgsz; 
      int index4 = ij - 1 + (imp - 1)*(p + 1) + p*pgsz; 
      d[index3] = ij*stheta*d[index4];
      if (ij > 1) 
	d[index3] -= sqc[ij + 2*(2*p + 1)]*
	  (d[index4 - pgsz]*cthtap + d[index4 + pgsz]*cthtan);
      d[index3] *= ww/sqc[ij + imp + 2*(2*p + 1)]; 

      for (im = 1; im <= ij; im++) {
	int index5 = ij + imp*(p + 1) + (im + p)*pgsz; 
	int index6 = ij - 1 + (imp - 1)*(p + 1) + (im + p)*pgsz; 
	d[index5] = d[index6 - pgsz]*cthtap*sqc[ij + im + 2*(2*p + 1)]; 
	if (im < ij - 1) 
	  d[index5] -= d[index6 + pgsz]*cthtan*sqc[ij - im + 2*(2*p + 1)]; 

	if (im < ij) 
	      d[index5] += d[index6]*stheta*sqc[ij + im + 2*p + 1]*
		sqc[ij - im + 2*p + 1];
	d[index5] *= ww/sqc[ij + imp + 2*(2*p + 1)];
      }
    }
  }
}


void vwts(fmm_param_t *fmm_param) {
  int nlambs    = fmm_param->nlambs; 
  double *rlams = fmm_param->rlams; 
  double *whts  = fmm_param->whts; 

  if (nlambs == 9) {
    rlams[0] = 0.99273996739714473469540223504736787e-01;
    rlams[1] = 0.47725674637049431137114652301534079e+00;
    rlams[2] = 0.10553366138218296388373573790886439e+01;
    rlams[3] = 0.17675934335400844688024335482623428e+01;
    rlams[4] = 0.25734262935147067530294862081063911e+01;
    rlams[5] = 0.34482433920158257478760788217186928e+01;
    rlams[6] = 0.43768098355472631055818055756390095e+01;
    rlams[7] = 0.53489575720546005399569367000367492e+01;
    rlams[8] = 0.63576578531337464283978988532908261e+01;
    whts[0]  = 0.24776441819008371281185532097879332e+00;
    whts[1]  = 0.49188566500464336872511239562300034e+00;
    whts[2]  = 0.65378749137677805158830324216978624e+00;
    whts[3]  = 0.76433038408784093054038066838984378e+00;
    whts[4]  = 0.84376180565628111640563702167128213e+00;
    whts[5]  = 0.90445883985098263213586733400006779e+00;
    whts[6]  = 0.95378613136833456653818075210438110e+00;
    whts[7]  = 0.99670261613218547047665651916759089e+00;
    whts[8]  = 0.10429422730252668749528766056755558e+01;
  } else if (nlambs == 18) { 
    rlams[0]  = 0.52788527661177607475107009804560221e-01;
    rlams[1]  = 0.26949859838931256028615734976483509e+00;
    rlams[2]  = 0.63220353174689392083962502510985360e+00;
    rlams[3]  = 0.11130756427760852833586113774799742e+01;
    rlams[4]  = 0.16893949614021379623807206371566281e+01;
    rlams[5]  = 0.23437620046953044905535534780938178e+01;
    rlams[6]  = 0.30626998290780611533534738555317745e+01;
    rlams[7]  = 0.38356294126529686394633245072327554e+01;
    rlams[8]  = 0.46542473432156272750148673367220908e+01;
    rlams[9]  = 0.55120938659358147404532246582675725e+01;
    rlams[10] = 0.64042126837727888499784967279992998e+01;
    rlams[11] = 0.73268800190617540124549122992902994e+01;
    rlams[12] = 0.82774009925823861522076185792684555e+01;
    rlams[13] = 0.92539718060248947750778825138695538e+01;
    rlams[14] = 0.10255602723746401139237605093512684e+02;
    rlams[15] = 0.11282088297877740146191172243561596e+02;
    rlams[16] = 0.12334067909676926788620221486780792e+02;
    rlams[17] = 0.13414920240172401477707353478763252e+02;
    whts[0]   = 0.13438265914335215112096477696468355e+00;
    whts[1]   = 0.29457752727395436487256574764614925e+00;
    whts[2]   = 0.42607819361148618897416895379137713e+00;
    whts[3]   = 0.53189220776549905878027857397682965e+00;
    whts[4]   = 0.61787306245538586857435348065337166e+00;
    whts[5]   = 0.68863156078905074508611505734734237e+00;
    whts[6]   = 0.74749099381426187260757387775811367e+00;
    whts[7]   = 0.79699192718599998208617307682288811e+00;
    whts[8]   = 0.83917454386997591964103548889397644e+00;
    whts[9]   = 0.87570092283745315508980411323136650e+00;
    whts[10]  = 0.90792943590067498593754180546966381e+00;
    whts[11]  = 0.93698393742461816291466902839601971e+00;
    whts[12]  = 0.96382546688788062194674921556725167e+00;
    whts[13]  = 0.98932985769673820186653756536543369e+00;
    whts[14]  = 0.10143828459791703888726033255807124e+01;
    whts[15]  = 0.10400365437416452252250564924906939e+01;
    whts[16]  = 0.10681548926956736522697610780596733e+01;
    whts[17]  = 0.11090758097553685690428437737864442e+01;
  }
}


void numthetahalf(fmm_param_t *fmm_param) {
  int *numfour = fmm_param->numfour; 
  int nlambs   = fmm_param->nlambs; 

  if (nlambs == 9) {
    numfour[0] = 2;
    numfour[1] = 4;
    numfour[2] = 4;
    numfour[3] = 6;
    numfour[4] = 6;
    numfour[5] = 4;
    numfour[6] = 6;
    numfour[7] = 4;
    numfour[8] = 2;
  } else if (nlambs == 18) {
    numfour[0]  = 4;
    numfour[1]  = 6;
    numfour[2]  = 6;
    numfour[3]  = 8;
    numfour[4]  = 8;
    numfour[5]  = 8;
    numfour[6]  = 10;
    numfour[7]  = 10;
    numfour[8]  = 10;
    numfour[9]  = 10;
    numfour[10] = 12;
    numfour[11] = 12;
    numfour[12] = 12;
    numfour[13] = 12;
    numfour[14] = 12;
    numfour[15] = 12;
    numfour[16] = 8;
    numfour[17] = 2;
  }
}


void numthetafour(fmm_param_t *fmm_param) {
  int *numphys = fmm_param->numphys; 
  int nlambs   = fmm_param->nlambs; 

  if (nlambs == 9) {
    numphys[0] = 4;
    numphys[1] = 8;
    numphys[2] = 12;
    numphys[3] = 16;
    numphys[4] = 20;
    numphys[5] = 20;
    numphys[6] = 24;
    numphys[7] = 8;
    numphys[8] = 2;
  } else if (nlambs == 18) {
    numphys[0]  = 6;
    numphys[1]  = 8;
    numphys[2]  = 12;
    numphys[3]  = 16;
    numphys[4]  = 20;
    numphys[5]  = 26;
    numphys[6]  = 30;
    numphys[7]  = 34;
    numphys[8]  = 38;
    numphys[9]  = 44;
    numphys[10] = 48;
    numphys[11] = 52;
    numphys[12] = 56;
    numphys[13] = 60;
    numphys[14] = 60;
    numphys[15] = 52;
    numphys[16] = 4;
    numphys[17] = 2;
  }
}


void rlscini(fmm_param_t *fmm_param) {
  int pterms    = fmm_param->pterms; 
  int nlambs    = fmm_param->nlambs; 
  int pgsz      = fmm_param->pgsz; 
  double *rlsc  = fmm_param->rlsc; 
  double *rlams = fmm_param->rlams; 

  double *factorial = calloc(2*pterms + 1, sizeof(double));
  double *rlampow = calloc(pterms + 1, sizeof(double));

  factorial[0] = 1;
  for (int i = 1; i <= 2*pterms; i++)
    factorial[i] = factorial[i-1]*sqrt(i);
 
  for (int nell = 0; nell < nlambs; nell++) {
    double rmul = rlams[nell];
    rlampow[0] = 1;
    for (int j = 1;  j <= pterms; j++)
      rlampow[j] = rlampow[j - 1]*rmul;      
    for (int j = 0; j <= pterms; j++) {
      for (int k = 0; k <= j; k++) {
	rlsc[j + k*(pterms + 1) + nell*pgsz] = rlampow[j]/
	  factorial[j - k]/factorial[j + k];
      }
    }    
  }
    
  free(factorial);
  free(rlampow);
}


void mkfexp(fmm_param_t *fmm_param) {
  int nlambs = fmm_param->nlambs; 
  int *numphys = fmm_param->numphys; 
  int *numfour = fmm_param->numfour; 
  double complex *fexpe = fmm_param->fexpe; 
  double complex *fexpo = fmm_param->fexpo; 
  double complex *fexpback = fmm_param->fexpback; 

  int nexte = 0; 
  int nexto = 0; 
  double m_pi = acos(-1); 

  for (int i = 0; i < nlambs; i++) {
    int nalpha = numphys[i]; 
    int nalpha2 = nalpha/2; 
    double halpha = 2.0*m_pi/nalpha; 
    for (int j = 1; j <= nalpha2; j++) {
      double alpha = (j - 1)*halpha; 
      for (int nm = 2; nm <= numfour[i]; nm += 2) {
	fexpe[nexte] = cexp((nm - 1)*_Complex_I*alpha);
	nexte++;
      }

      for (int nm = 3; nm <= numfour[i]; nm += 2) {
	fexpo[nexto] = cexp((nm - 1)*_Complex_I*alpha);
	nexto++;
      }
    }
  }

  int next = 0; 
  for (int i = 0; i < nlambs; i++) {
    int nalpha = numphys[i]; 
    int nalpha2 = nalpha/2; 
    double halpha = 2.0*m_pi/nalpha; 
    for (int nm = 3; nm <= numfour[i]; nm += 2) {
      for (int j = 1; j <= nalpha2; j++) {
	double alpha = (j - 1)*halpha; 
	fexpback[next] = cexp(-(nm - 1)*_Complex_I*alpha);
	next++;
      }
    }

    for (int nm = 2; nm <= numfour[i]; nm += 2) {
      for (int j = 1; j <= nalpha2; j++) {
	double alpha = (j - 1)*halpha; 
	fexpback[next] = cexp(-(nm - 1)*_Complex_I*alpha);
	next++;
      }
    }
  }
}


void mkexps(fmm_param_t *fmm_param) {
  int nlambs = fmm_param->nlambs; 
  int *numphys = fmm_param->numphys;
  double *rlams = fmm_param->rlams; 
  double complex *xs = fmm_param->xs; 
  double complex *ys = fmm_param->ys; 
  double *zs = fmm_param->zs; 
  int ntot = 0; 
  double m_pi = acos(-1); 
  for (int nell = 0; nell < nlambs; nell++) {
    double hu = 2.0*m_pi/numphys[nell]; 
    for (int mth = 0; mth < numphys[nell]/2; mth++) {
      double u = mth*hu; 
      int ncurrent = 3*(ntot + mth);
      zs[ncurrent]     = exp(-rlams[nell]);
      zs[ncurrent + 1] = zs[ncurrent]*zs[ncurrent]; 
      zs[ncurrent + 2] = zs[ncurrent]*zs[ncurrent + 1];
      xs[ncurrent]     = cexp(_Complex_I*cos(u)*rlams[nell]);
      xs[ncurrent + 1] = xs[ncurrent]*xs[ncurrent];
      xs[ncurrent + 2] = xs[ncurrent + 1]*xs[ncurrent]; 
      ys[ncurrent]     = cexp(_Complex_I*sin(u)*rlams[nell]);
      ys[ncurrent + 1] = ys[ncurrent]*ys[ncurrent]; 
      ys[ncurrent + 2] = ys[ncurrent + 1]*ys[ncurrent]; 
    }
    ntot += numphys[nell]/2; 
  }
}
