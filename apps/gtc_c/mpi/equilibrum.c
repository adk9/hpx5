#include <assert.h>
#include <math.h>
#include "bench_gtc.h"

/* set up temprature and density profile Ti_0, n_0 (rtemi, rden), the gradient of 
   temprature and density profile dTi_0/Ti_0, dn_0/n_0 (kapati, kapan) at backgroud */
int equilibrium(gtc_bench_data_t *gtc_input) {
  
  gtc_global_params_t *params;
  gtc_field_data_t *field_data;

  int mpsi, background_case, nbound;

  real *rtemi; real *rden; real *kapati; real *kapan; real *temp; real *dtemp;

  real kappan, kappati, rc, rw, a0, deltar, gyroradius, aion, qion;
  real vthi, sbound;

  params            = &(gtc_input->global_params);
  field_data        = &(gtc_input->field_data);

  mpsi = params->mpsi;
  kappan = params->kappan;
  kappati = params->kappati;
  rc = params->rc;
  rw = params->rw;
  aion = params->aion;
  qion = params->qion;
  a0 = params->a0;
  deltar = params->deltar;
  gyroradius = params->gyroradius;
  nbound = params->nbound;
  background_case = params->background_case;

  rtemi = field_data->rtemi;
  rden = field_data->rden;
  kapati = field_data->kapati;
  kapan = field_data->kapan;
  temp = field_data->temp;
  dtemp = field_data->dtemp;

  sbound = 1.0;
  if (nbound == 0)
    sbound = 0.0;
  vthi = gyroradius*fabs(qion)/aion;
  
#pragma omp parallel for
  for (int i=0; i<mpsi+1; i++){

    real rad = a0 + i*deltar;

    if (background_case==0){
#if FLAT_PROFILE
      rtemi[i] = 1.0;
      rden[i] = 1.0;
#else
      printf("PROFILE_SHAPE==0 FLAT_PROFILE==0 not available\n");
      exit (1);
#endif
      real rfac1 = rw *(rad - rc);
      real rfac2 = rfac1*rfac1;
      real rfac3 = rfac2*rfac2*rfac2;
      real rfac = exp(-1.0*rfac3);
      real kappa = 1.0 - sbound + sbound * rfac;

      kapati[i] = kappati*kappa;
      kapan[i] = kappan*kappa;
      
    }
    else if (background_case==1){
#if FLAT_PROFILE
      rtemi[i] = 1.0;
      rden[i] = 1.0;
#else
      rtemi[i] = exp(-kappati*tanh((rad - rc)*rw)/rw);
      rden[i] = exp(-kappan*tanh((rad - rc)*rw)/rw);
#endif
      real rfac1 = rw *(rad - rc);
      real rfac2 = tanh(rfac1)*tanh(rfac1);
      real rfac = 1.0 - rfac2;
      real kappa = 1.0 - sbound + sbound * rfac;

      kapati[i] = kappati*kappa;
      kapan[i] = kappan*kappa;
     
    }
    else {
      printf("others not avaialble\n");
    }
    
    temp[i]  = 1.0;
    dtemp[i] = 0.0;
    temp[i]  = 1.0/(temp[i] * rtemi[i] * aion * vthi * vthi);

  }

  // normalization for on-axis quantities
  real rtemi_a0 = rtemi[0];
  real rtemi_a0_inv = 1.0/rtemi[0];
  real rden_a0_inv = 1.0/rden[0];
#pragma omp parallel for
  for (int i=0; i<mpsi+1; i++){
    rtemi[i] *= rtemi_a0_inv;
    rden[i] *= rden_a0_inv;
    temp[i] *= rtemi_a0;
  }

  return 0;

}
