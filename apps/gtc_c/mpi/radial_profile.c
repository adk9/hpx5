#include <assert.h>
#include <math.h>
#include "bench_gtc.h"
#include "RngStream.h"

int radial_profile(gtc_bench_data_t *gtc_input){

  gtc_global_params_t *global_params;
  gtc_particle_data_t *particle_data;
  gtc_field_data_t *field_data;
  gtc_particle_decomp_t *parallel_decomp;

  real *z0, *z1, *z2, *z3, *z4, *z5;
  real *z05;
  real a0, aion, qion, vthi, delr, deltar, b, cmratio, gyroradius;
  int mpsi, mi;

  real *zonali, *zonali0, *phi00, *phip00, *pmarki, *rden, *rtemi, *kapati, *kapan;

  global_params     = &(gtc_input->global_params);
  particle_data     = &(gtc_input->particle_data);
  field_data        = &(gtc_input->field_data);
  parallel_decomp   = &(gtc_input->parallel_decomp);

  mi = global_params->mi;
  mpsi = global_params->mpsi;
  a0 = global_params->a0;
  deltar = global_params->deltar;
  aion = global_params->aion;
  qion = global_params->qion;
  gyroradius = global_params->gyroradius;
  
  zonali = field_data->zonali; zonali0 = field_data->zonali0;
  phi00 = field_data->phi00;
  phip00 = field_data->phip00;
  pmarki = field_data->pmarki;
  rden = field_data->rden;
  rtemi = field_data->rtemi;
  kapati = field_data->kapati;
  kapan = field_data->kapan;

  z0 = particle_data->z0; z1 = particle_data->z1;
  z2 = particle_data->z2; z3 = particle_data->z3;
  z4 = particle_data->z4; z5 = particle_data->z5;
  z05 = particle_data->z05;

  delr = 1.0/deltar;
  cmratio = qion/aion;
  vthi = gyroradius*fabs(qion)/aion;

  real *rdentmp = malloc(sizeof(real)*(mpsi+1));
  real *rtemitmp = malloc(sizeof(real)*(mpsi+1));
  real *rmarkertmp = malloc(sizeof(real)*(mpsi+1));
  real *drtemitmp = malloc(sizeof(real)*(mpsi+1));
  real *adum = malloc(sizeof(real)*(mpsi+1));
  real *adum2 = malloc(sizeof(real)*(mpsi+1));
  real *adum3 = malloc(sizeof(real)*(mpsi+1));
  
#pragma omp parallel for
  for (int i=0; i<mpsi+1; i++){
    rdentmp[i] = 0.0;
    rtemitmp[i] = 0.0;
    rmarkertmp[i] = 0.0;
    drtemitmp[i] = 0.0;
    adum[i] = 0.0;
    adum2[i] = 0.0;
    adum3[i] = 0.0;
  }

  for (int m=0; m<mi; m++){
    real zion0m = z0[m];
    real zion1m = z1[m];
    real zion3m = z3[m];
    real zion5m = z5[m];
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

    real upara = zion3m*b*cmratio;
    real energy = 0.5*aion*upara*upara + zion5m*zion5m*b;

    rmarkertmp[ii] += wp0;
    rmarkertmp[ii+1] += wp1;
    rdentmp[ii] += wp0*weight;
    rdentmp[ii+1] += wp1*weight;
    rtemitmp[ii] += wp0*energy*weight;
    rtemitmp[ii+1] += wp1*energy*weight;
  }

  MPI_Reduce(rdentmp, adum, mpsi+1, MPI_MYREAL, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(rtemitmp, adum2, mpsi+1, MPI_MYREAL, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(rmarkertmp, adum3, mpsi+1, MPI_MYREAL, MPI_SUM, 0, MPI_COMM_WORLD);

  if (parallel_decomp->mype==0){
    real normalfac = 1.5*aion*vthi*vthi;
    normalfac = 1.0/normalfac;
#pragma omp parallel for
    for (int i=0; i<mpsi+1; i++){
#if UNIFORM_PARTICLE_LOADING
      rmarkertmp[i] = adum3[i]*pmarki[i];
      rdentmp[i] = adum[i]*pmarki[i];
#else
      rmarkertmp[i] = adum3[i]*pmarki[i]*rden[i];
      rdentmp[i] = adum[i]*pmarki[i]*rden[i];
#endif
      rtemitmp[i] = adum2[i]*pmarki[i]*normalfac/rdentmp[i];
    }
    for (int i=1; i<mpsi; i++){
	drtemitmp[i] = 0.5*delr*(rtemitmp[i+1]-rtemitmp[i-1])/rtemitmp[i];
    }
  
    char filename[50];
    sprintf(filename, "RADIAL_PROFILE/radial_profile%d.txt", istep); 
    FILE *fp;
    fp = fopen(filename, "w");
    for (int i=0; i<mpsi+1; i++){
      fprintf(fp, "%d %e %e %e %e %e %e %e\n", i, rdentmp[i]-rden[i], rtemitmp[i]-rtemi[i], rmarkertmp[i], drtemitmp[i], zonali[i], phi00[i], phip00[i]);
    }
    fclose(fp);
  }

  MPI_Barrier(MPI_COMM_WORLD);
  
  free(rdentmp);
  free(rtemitmp);
  free(drtemitmp);
  free(adum);
  free(adum2);
  
  return 0;
}
