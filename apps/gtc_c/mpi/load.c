#include <assert.h>
#include <math.h>
#include "bench_gtc.h"
#include "RngStream.h"

int load(gtc_bench_data_t *gtc_input){

  gtc_global_params_t *global_params;
  gtc_particle_data_t *particle_data;
  gtc_particle_decomp_t *parallel_decomp;
  gtc_radial_decomp_t *radial_decomp;
  gtc_field_data_t *field_data;   

  real c0, c1, c2, d1, d2, d3, rmi;
  real zetamin, zetamax, a0, a1, umax, rw, rc;
  real pi, pi2_inv, deltar, delr, deltaz, aion, qion, gyroradius, vthi;
  real zt1, zt2, z3tmp, w_initial, cmratio;
  int mpsi, mi, mype, numberpe, npartdom, nproc_radiald, myrank_radiald, ntoroidal, mzeta;
  int i, m;  //int ntracer;
  int ipsi_in, ipsi_out, ipsi_nover_in, ipsi_nover_out, igrid_in;

  RngStream rs;
  double *rng_seed;

  real *z0, *z1, *z2, *z3, *z4, *z5, *z6;
  real *z00, *z01, *z03, *z05;

  int *mtheta, *igrid;

  real *temp, *rtemi, *rden, *pmarki, *qtinv, *deltat;
  real *markeri;
  real *adum;

  global_params     = &(gtc_input->global_params);
  particle_data     = &(gtc_input->particle_data);
  parallel_decomp   = &(gtc_input->parallel_decomp);
  radial_decomp     = &(gtc_input->radial_decomp);
  field_data        = &(gtc_input->field_data);

  mi = global_params->mi;
  mpsi = global_params->mpsi;
  pi = global_params->pi;
  deltar = global_params->deltar;
  umax = global_params->umax;
  zetamax = global_params->zetamax;
  zetamin = global_params->zetamin;
  a0 = global_params->a0;
  a1 = global_params->a1;
  aion = global_params->aion;
  qion = global_params->qion;
  gyroradius = global_params->gyroradius;
  rw = global_params->rw;
  rc = global_params->rc;
  deltaz = global_params->deltaz;
  mzeta = global_params->mzeta;

  mype = parallel_decomp->mype;
  numberpe = parallel_decomp->numberpe;
  npartdom = parallel_decomp->npartdom;
  ntoroidal = parallel_decomp->ntoroidal;
  nproc_radiald = radial_decomp->nproc_radiald;
  myrank_radiald = radial_decomp->myrank_radiald;
  ipsi_in   = radial_decomp->ipsi_in;
  ipsi_out  = radial_decomp->ipsi_out;
  ipsi_nover_in = radial_decomp->ipsi_nover_in;
  ipsi_nover_out = radial_decomp->ipsi_nover_out;
  igrid_in = radial_decomp->igrid_in;

  z0 = particle_data->z0; z1 = particle_data->z1;
  z2 = particle_data->z2; z3 = particle_data->z3;
  z4 = particle_data->z4; z5 = particle_data->z5;
#if TWO_WEIGHTS
  z6 = particle_data->z6;
#endif
  z00 = particle_data->z00; z01 = particle_data->z01;
  z03 = particle_data->z03; z05 = particle_data->z05;

  temp = field_data->temp;
  rtemi = field_data->rtemi;
  rden = field_data->rden;
  pmarki = field_data->pmarki;
  qtinv = field_data->qtinv;
  markeri = field_data->markeri;
  igrid = field_data->igrid;
  mtheta = field_data->mtheta;
  deltat = field_data->deltat;

  /* # of marker per grid, Jacobian */
  for (int i=ipsi_in; i<ipsi_out+1; i++) {
    real r = a0+deltar*i;
    for (int j=1; j<mtheta[i]+1; j++) {
      int ij = igrid[i]+j;
      for (int k=1; k<mzeta+1; k++) {
	real zdum = zetamin + k*deltaz;
	real tdum = j*deltat[i]+zdum*qtinv[i];
#if UNIFORM_PARTICLE_LOADING
	markeri[(ij-igrid_in)*mzeta+k-1] = r*deltat[i]*pow((1.0 + r*cos(tdum)), 2);
#else
	markeri[(ij-igrid_in)*mzeta+k-1] = rden[i]*r*deltat[i]*pow((1.0 + r*cos(tdum)),2);
#endif
	if (i==0||i==mpsi)
          markeri[(ij-igrid_in)*mzeta+k-1] /= 2.0;

	if (i>=ipsi_nover_in&&i<=ipsi_nover_out)
	  pmarki[i] = pmarki[i]+markeri[(ij-igrid_in)*mzeta+k-1];
      }
    }
  }

  adum = (real *)_mm_malloc((mpsi+1)*sizeof(real), IDEAL_ALIGNMENT);
  assert(adum != NULL);
  MPI_Allreduce(pmarki, adum, mpsi+1, MPI_MYREAL, MPI_SUM, parallel_decomp->partd_comm);
  real pdum=0.0;
#pragma omp parallel for reduction(+:pdum)
  for (int i=0; i<mpsi+1; i++){
    pmarki[i] = adum[i];
    pdum += pmarki[i];
  }

  real tdum = ((real)mi)*((real)npartdom);
  for (int i=ipsi_in; i<ipsi_out+1; i++){
    for (int j=1; j<mtheta[i]+1; j++) {
      int ij = igrid[i]+j;
      for (int k=1; k<mzeta+1; k++) {
	markeri[(ij-igrid_in)*mzeta+k-1] = tdum*markeri[(ij-igrid_in)*mzeta+k-1]/pdum; // # of marker per cell
	markeri[(ij-igrid_in)*mzeta+k-1] = 1.0/markeri[(ij-igrid_in)*mzeta+k-1]; // avoid divide operation
      }
    }

    for (int k=1; k<mzeta+1; k++) {
      markeri[(igrid[i]-igrid_in)*mzeta+k-1] = markeri[(igrid[i]+mtheta[i]-igrid_in)*mzeta+k-1];
    }
  }

#pragma omp parallel for 
  for (int i=0; i<mpsi+1; i++){
    real tmp = ((real)mi)*((real)npartdom)*((real)ntoroidal)*pmarki[i]/pdum;
    pmarki[i] = 1.0/tmp;
  }

  real volumn_frac = (a1*a1-a0*a0)/nproc_radiald;
  real x2_l = a0*a0 + myrank_radiald*volumn_frac;

  real rmin = sqrt(a0*a0 + myrank_radiald*volumn_frac);
  real rmax = sqrt(a0*a0 + (myrank_radiald+1)*volumn_frac);

  /* load function */
  c0 = 2.515517;
  c1 = 0.802853;
  c2 = 0.010328;
  d1 = 1.432788;
  d2 = 0.189269;
  d3 = 0.001308;
  //#if UNIFORM_PARTICLE_LOADING
  //  rmi = 1.0/mi;
  //#else
  //  rmi = 1.0/(mi*npartdom);
  //#endif
  pi2_inv = 0.5/pi;
  delr =  1.0/deltar;
  w_initial = 1.0e-3;
  cmratio= qion/aion;

  /* radial: uniformly distributed in r^2, later transform to psi */
  /*
    for (m=0; m<mi; m++) {
#if UNIFORM_PARTICLE_LOADING
        z0[m] = sqrt(a0*a0+(((real) m+0.5)*(a1*a1-a0*a0)*rmi));
#else
        z0[m] = sqrt(a0*a0+((real) (m+1+myrank_partd*mi)-0.5)*(a1*a1-a0*a0)*rmi);
        // this doesn't result in correct physics, fix it!
        //z0[m] = sqrt(a0*a0+((real) (myrank_radial_partd+m+1+myrank_radiald*nproc_radial_partd*mi)-0.5)*(a1*a1-a0*a0)*rmi);
#endif
    }
  */

  /* Initialize RNG */
  rng_seed = (double *) malloc(6 * sizeof(double));
  assert(rng_seed != NULL);
  rng_seed[0] = 12345; rng_seed[1] = 12345; rng_seed[2] = 12345;
  rng_seed[3] = 12345; rng_seed[4] = 12345; rng_seed[5] = 12345;

  RngStream_ParInit(mype, numberpe, 0, 1, 1, rng_seed);
  rs = RngStream_CreateStream("", rng_seed);

  for (m=0; m<mi; m++) {
    z0[m] = (real) RngStream_RandU01(rs);
    z1[m] = (real) RngStream_RandU01(rs);
    z2[m] = (real) RngStream_RandU01(rs);
    z3[m] = (real) RngStream_RandU01(rs);
    z4[m] = (real) RngStream_RandU01(rs);
    z5[m] = (real) RngStream_RandU01(rs);
  }

  // randomly uniform distribute in r^2
  for (m=0; m<mi; m++) {
    //if (m<10)
    //  printf("mype=%d m=%d z0=%e\n",parallel_decomp->mype,m, z0[m]); 
    z0[m] = sqrt(x2_l + z0[m]*volumn_frac);
  }

  // distribute in theta
  for (m=0; m<mi; m++){
    z1[m] = 2.0*pi*(z1[m]-0.5);
    z01[m] = z1[m];
  }

  for (i=0; i<10; i++) {
    for (m=0; m<mi; m++) {
      z1[m] = z01[m]-2.0*z0[m]*sin(z1[m]);
    }
  }

  for (m=0; m<mi; m++) {
    z1[m] = z1[m]*pi2_inv+10.0;
    z1[m] = 2.0 * pi * (z1[m] - (int) z1[m]);
  }

  // distribute in v_para
  for (m=0; m<mi; m++) {
    z3tmp  = z3[m];
    z3[m]  = z3[m]-0.5;
#if NORMAL_LOADING
    z03[m] = ((z3[m] > 0) ? 1.0 : -1.0);
    z3[m]  = sqrt(max(1e-20,log(1.0/max(1e-20, pow(z3[m],2)))));
    z3[m]  = z3[m]-(c0+c1*z3[m]+c2*pow(z3[m],2))/
      (1.0+d1*z3[m]+d2*pow(z3[m],2)+d3*pow(z3[m],3));
    if (z3[m] > umax)
      z3[m] = z3tmp;
#else
    z3[m] = z3[m]*2.0*umax;
#endif
  }

  // distribute in v_perp
  for (m=0; m<mi; m++) {
    z2[m] = zetamin + (zetamax-zetamin)*z2[m];
#if NORMAL_LOADING
    z3[m] = z03[m]*min(umax,z3[m]);
#endif

    i = abs_min_int(mpsi-1, ((int)((z0[m]-a0)*delr)));

#if DELTAF
    z4[m] =
      2.0*w_initial*(z4[m]-0.5)*(1.0+cos(z1[m]));
    //z4[m] = 0.001*sin(2.0*pi*(z0[m]-a0)/(a1-a0)); // for Rosenbloth test
    //z4[m] = 0.000001*cos(10.0*z2[m] - 10.0*((z2[m]*qtinv[i]-z1[m])/qtinv[i]));
#else
    z4[m] =
      2.0*w_initial*(z4[m]-0.5)*(1.0+cos(z1[m]));
    //z4[m] = 0.001*sin(2.0*pi*(z0[m]-a0)/(a1-a0)); // for Rosenbloth test
    //z4[m] = 0.000001*cos(10.0*z2[m] - 10.0*((z2[m]*qtinv[i]-z1[m])/qtinv[i]));
#endif

#if NORMAL_LOADING
    zt1 = max(1.0e-20,z5[m]);
    /* (z5[m] > 1.0e-20) ? z5[m] : 1.0e-20; */
    zt2 = min(2.0*umax*umax,-2.0*log(zt1));
    /* (umax*umax < -log(zt1)) ? umax*umax : -log(zt1); */
    z5[m] = max(2.0e-20,zt2);
#else
    z5[m] = z5[m]*2.0*umax*umax;
#endif
  }

  vthi = gyroradius*fabs(qion)/aion;
  for (m=0; m<mi; m++) {
    z00[m] = 1.0/(1.0+z0[m]*cos(z1[m]));
    i = abs_min_int(mpsi-1, ((int)((z0[m]-a0)*delr)));

#if !SQRT_PRECOMPUTED
    z0[m] = 0.5 * z0[m]*z0[m];
#endif
    z3[m] = vthi*z3[m]*aion/(qion*z00[m]);
    z5[m] = sqrt(0.5*aion*vthi*vthi*z5[m]/z00[m]);

#if !FLAT_PROFILE
    z3[m] = z3[m]*sqrt(rtemi[i]);
    z5[m] = z5[m]*sqrt(rtemi[i]);
#endif
  }

  // z05[m] particle weights
  // PROFILE_SHAPE==1
  // T(r) = T_0 exp(-k_w*k_T*tanh((r-r_c)/r_w))
  // |grad(T)|/T_0 = k_T*sech^2((r-r_c)/r_w))
  // n(r) = n_0 exp(-k_w*k_n*tanh((r-r_c)/r_w))
  // |grad(n)|/n_0 = k_n*sech^2((r-r_c)/r_w)) 
  // PROFILE_SHAPE==0
  // |grad(T)|/T_0 = k_T*exp(-((r-r_c)/r_w)^6) 
  // |grad(n)|/n_0 = k_n*exp(-((r-r_c)/r_w)^6) 
  // f = n(r)/(2*pi*T(r))^(3/2)*exp(-energy/T(r))
  
#if NORMAL_LOADING

#if FLAT_PROFILE
  for (m=0; m<mi; m++) {
    z05[m] = 1.0;
  }
#else

#if PROFILE_SHAPE == 1 
  for (m=0; m<mi; m++) {
    real zion0m = z0[m];
#if SQRT_PRECOMPUTED
    real r = zion0m;
#else
    real r = sqrt(2.0*zion0m);
#endif

    int ii = abs_min_int(mpsi-1, ((int)((r-a0)*delr)));
    real wp0 = ((real)(ii+1)) - (r-a0)*delr;
    real wp1 = 1.0 - wp0;

    //    real rfac1 = rw * (r-rc);
    //    real rden = exp(-global_params->kappan*tanh(rfac1)/rw);
    // normalize at the axis 
    //    rden /= exp(-global_params->kappan*tanh(rw*(a0-rc))/rw);
    real rdentmp = wp0*rden[ii] + wp1*rden[ii+1];
    rdentmp /= rden[0];

#if UNIFORM_PARTICLE_LOADING 
    z05[m] = rdentmp;
#else
    z05[m] = 1.0;
#endif
  }
#else
  printf("PROFILE_SHAPE==0 FLAT_PROFILE==0 not available\n");
  exit (1);
#endif

#endif

#else
  real sum = 0.0;
  real velocity_vol = 4.0*umax*umax*umax;

  for (m=0; m<mi; m++) {
    real zion0m = z0[m];
    real zion1m = z1[m];
    real zion3m = z3[m];
    real zion5m = z5[m];
    real b = z00[m];
#if SQRT_PRECOMPUTED
    real r = zion0m;
#else
    real r = sqrt(2.0*zion0m);
#endif
    int ii = abs_min_int(mpsi-1, ((int)((r-a0)*delr)));

    real wp0 = ((real)(ii+1)) - (r-a0)*delr;
    real wp1 = 1.0 - wp0;

    real tem = wp0 * temp[ii] + wp1 * temp[ii+1];
    real upara = zion3m*b*cmratio;
    real energy = 0.5*aion*upara*upara + zion5m*zion5m*b;

#if FLAT_PROFILE
    z05[m] = velocity_vol*exp(-energy*tem)/(2.0*sqrt(2.0*pi));
#else

#if PROFILE_SHAPE == 0 // exp(x^6) use flat profile 
    printf("PROFILE_SHAPE==0 FLAT_PROFILE==0 not available\n");
    exit (1);
#elif PROFILE_SHAPE == 1
    real rfac1 = rw * (r-rc);
    real rden = exp(-global_params->kappan*tanh(rfac1)/rw);
    rden /= exp(-global_params->kappan*tanh(rw*(a0-rc))/rw);
#if UNIFORM_PARTICLE_LOADING 
    z05[m] = rden*velocity_vol*exp(-energy*tem)/(2.0*sqrt(2.0*pi));
#else
    z05[m] = 1.0;
#endif

#endif    

#endif

    sum += z05[m];
  }

  sum = sum/(real)mi;
  real sum_total = 0.0;
  MPI_Allreduce(&sum, &sum_total, 1, MPI_MYREAL, MPI_SUM, MPI_COMM_WORLD);
  if (mype==0) printf("UNIFORM LOADING: mype=%d sum=%e sum_average=%e\n", mype, sum, sum_total/(real)numberpe);

#endif


#define CORRECTION 1
#if CORRECTION
  for (int i=0; i<mpsi+1; i++){
    adum[i] = 0.0;
  }

  for (int m=0; m<mi; m++){
    real zion0m = z0[m];
    real zion1m = z1[m];
    real weight = z05[m];

#if SQRT_PRECOMPUTED
    real r = zion0m;
#else
    real r = sqrt(2.0*zion0m);
#endif

    real b = 1.0/(1.0 + r*cos(zion1m));

    int ii = abs_min_int(mpsi-1, ((int)((r-a0)*delr)));

    real wp0 = ((real)(ii+1)) - (r-a0)*delr;
    real wp1 = 1.0 - wp0;

    //adum[ii] += wp0*weight;
    //adum[ii+1] += wp1*weight;
    adum[ii] += wp0;
    adum[ii+1] += wp1;
  }

  real *adum2 = (real *) malloc((mpsi+1)*sizeof(real));
  assert(adum2 != NULL);
  MPI_Allreduce(adum, adum2, mpsi+1, MPI_MYREAL, MPI_SUM, MPI_COMM_WORLD);
  for (int i=0; i<mpsi+1; i++){
    //adum[i] = adum2[i]*pmarki[i] - rden[i];
    adum[i] = adum2[i]*pmarki[i] - 1.0;
  }
  free(adum2);

  for (int m=0; m<mi; m++){
    real zion0m = z0[m];
    real zion1m = z1[m];
    real weight = z05[m];

#if SQRT_PRECOMPUTED
    real r = zion0m;
#else
    real r = sqrt(2.0*zion0m);
#endif

    real b = 1.0/(1.0 + r*cos(zion1m));

    int ii = abs_min_int(mpsi-1, ((int)((r-a0)*delr)));

    real wp0 = ((real)(ii+1)) - (r-a0)*delr;
    real wp1 = 1.0 - wp0;

    z05[m] = weight*(1.0-(adum[ii]*wp0+adum[ii+1]*wp1));
  }
#endif


#if TWO_WEIGHTS
#if HU_KROMMES // c=f_0/g
#pragma omp parallel for
  for (m=0; m<mi; m++){
    z6[m] = 1.0;
  }
#else // c=f/g
#pragma omp parallel for
  for (m=0; m<mi; m++){
    z6[m] = 1.0+z4[m];
  }
#endif
#endif

  /*
  ntracer = 0; 
  if (mype==0) {
    z0[ntracer] = 0.5*pow((0.5*(a0+a1)),2);
    z1[ntracer] = 0.0;
    z2[ntracer] = 0.5*(zetamin+zetamax);
    z3[ntracer] = 0.5*vthi*aion/qion;
    z4[ntracer] = 0.0;
    z5[ntracer] = sqrt(aion*vthi*vthi);
    }            
  */
  
  RngStream_DeleteStream(rs);
  free(rng_seed);
  free(adum);

  return 0;
}
