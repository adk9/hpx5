/// ----------------------------------------------------------------------------
/// @file fmm-action.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief Implementations of FMM actions
/// ----------------------------------------------------------------------------

#include <math.h>
#include <stdlib.h>
#include "fmm.h"

hpx_action_t _fmm_main; 
hpx_action_t _construct_dag; 

hpx_addr_t sources; 
hpx_addr_t charges; 
hpx_addr_t targets; 
hpx_addr_t potential;
hpx_addr_t field; 
hpx_addr_t fmm_dag; 

//fmm_dag_t *fmm_dag_pinned; 

int _fmm_main_action(void *args) {
  fmm_config_t *fmm_cfg = (fmm_config_t *)args; 

  // generate test data according to the configuration
  int nsources = fmm_cfg->nsources; 
  int ntargets = fmm_cfg->ntargets; 
  int datatype = fmm_cfg->datatype; 
  int s        = fmm_cfg->s; 

  // allocate memory to hold source and target point info
  sources = hpx_gas_alloc(nsources * 3, sizeof(double)); 
  charges = hpx_gas_alloc(nsources, sizeof(double));
  targets = hpx_gas_alloc(ntargets * 3, sizeof(double)); 
  potential = hpx_gas_alloc(ntargets, sizeof(double));
  field = hpx_gas_alloc(ntargets * 3, sizeof(double));

  // populate test data
  double *sources_pinned = NULL;
  double *charges_pinned = NULL; 
  double *targets_pinned = NULL; 
  hpx_gas_try_pin(sources, (void **)&sources_pinned);
  hpx_gas_try_pin(charges, (void **)&charges_pinned);
  hpx_gas_try_pin(targets, (void **)&targets_pinned); 

  double pi = acos(-1);
  if (datatype == 1) {
    for (int i = 0; i < nsources; i++) {
      int j = 3*i;
      charges_pinned[i]     = 1.0*rand()/RAND_MAX - 0.5;
      sources_pinned[j]     = 1.0*rand()/RAND_MAX - 0.5;
      sources_pinned[j + 1] = 1.0*rand()/RAND_MAX - 0.5;
      sources_pinned[j + 2] = 1.0*rand()/RAND_MAX - 0.5;
    }

    for (int i = 0; i < ntargets; i++) {
      int j = 3*i;
      targets_pinned[j]     = 1.0*rand()/RAND_MAX - 0.5;
      targets_pinned[j + 1] = 1.0*rand()/RAND_MAX - 0.5;
      targets_pinned[j + 2] = 1.0*rand()/RAND_MAX - 0.5;
    }
  } else if (datatype == 2) {
    for (int i = 0; i < nsources; i++) {
      int j = 3*i;
      double theta = 1.0*rand()/RAND_MAX*pi;
      double phi = 1.0*rand()/RAND_MAX*pi*2;

      charges_pinned[i]     = 1.0*rand()/RAND_MAX - 0.5;
      sources_pinned[j]     = sin(theta)*cos(phi);
      sources_pinned[j + 1] = sin(theta)*sin(phi);
      sources_pinned[j + 2] = cos(theta);
    }

    for (int i = 0; i < ntargets; i++) {
      int j = 3*i;
      double theta = 1.0*rand()/RAND_MAX*pi;
      double phi = 1.0*rand()/RAND_MAX*pi*2;
      targets_pinned[j]     = sin(theta)*cos(phi);
      targets_pinned[j + 1] = sin(theta)*sin(phi);
      targets_pinned[j + 2] = cos(theta);
    }
  }

  // construct DAG
  fmm_dag = hpx_gas_alloc(1, sizeof(fmm_dag_t) + 
			  sizeof(int) * (nsources + ntargets)); 

  /*

  hpx_gas_try_pin(fmm_dag, (void *)&fmm_dag_pinned); 
  fmm_dag_pinned->mapsrc = &fmm_dag_pinned->mapping[0]; 
  fmm_dag_pinned->maptar = &fmm_dag_pinned->mapping[nsources];  
  for (int i = 0; i < nsources; i++) 
    fmm_dag_pinned->mapsrc[i] = i; 

  for (int i = 0; i < ntargets; i++) 
    fmm_dag_pinned->maptar[i] = i; 
  */

  // unpin source and target point info
  hpx_gas_unpin(sources); 
  hpx_gas_unpin(charges); 
  hpx_gas_unpin(targets);


  // cleanup 
  hpx_gas_global_free(sources); 
  hpx_gas_global_free(charges);
  hpx_gas_global_free(targets);
  hpx_gas_global_free(potential);
  hpx_gas_global_free(field); 
  hpx_gas_global_free(fmm_dag); 

  hpx_shutdown(0); 
}

int _construct_dag_action(void *args) {
  return 0; 
}
