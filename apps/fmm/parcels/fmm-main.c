/// ----------------------------------------------------------------------------
/// @file fmm-main.c
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief  Main file for FMM 
/// ----------------------------------------------------------------------------

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "hpx/hpx.h"
#include "fmm.h"

int nsources; 
int ntargets; 
int datatype; 
int accuracy; 
int s; 

hpx_action_t _fmm_main;
hpx_action_t _init_sources; 
hpx_action_t _init_charges; 
hpx_action_t _init_targets; 
hpx_action_t _init_mapsrc; 
hpx_action_t _init_maptar; 
hpx_action_t _init_param; 
hpx_action_t _init_source_root; 
hpx_action_t _init_target_root; 

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: fmm [options]\n"
      "\t-c, number of cores to run on\n"
      "\t-t, number of scheduler threads\n"
      "\t-n, number of source points\n"
      "\t-m, number of target points\n"
      "\t-a, accuracy requirement, only 3 and 6 are valid\n"
      "\t-d, distribution of the particles\n"
      "\t    type-1 data is uniformly distributed inside a box\n"
      "\t    type-2 data is uniformly distributed over a sphere\n"
      "\t-s, maximum number of points allowed per leaf box\n"
      "\t-D, wait for debugger\n");
}

int main(int argc, char *argv[]) {
  hpx_config_t hpx_cfg = {
    .cores = 0,
    .threads = 0,
    .stack_bytes = 8192
  };

  nsources = 10000;
  ntargets = 10000;
  datatype = 1;
  accuracy = 3;
  s        = 40;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:n:m:a:d:s:D:h")) != -1) {
    switch (opt) {
    case 'c':
      hpx_cfg.cores = atoi(optarg);
      break;
    case 't':
      hpx_cfg.threads = atoi(optarg);
      break;
    case 'n':
      nsources = atoi(optarg);
      break;
    case 'm':
      ntargets = atoi(optarg);
      break;
    case 'a':
      accuracy = atoi(optarg);
      break;
    case 'd':
      datatype = atoi(optarg);
      break;
    case 's':
      s        = atoi(optarg);
      break;
    case 'h':
      _usage(stdout);
      return 0;
    case '?':
    default:
      _usage(stderr);
      return -1;
    }
  }

  // init hpx runtime
  int e = hpx_init(&hpx_cfg);
  if (e) {
    fprintf(stderr, "HPX:failed to initialize.\n");
    return e;
  }

  // register actions
  _fmm_main         = HPX_REGISTER_ACTION(_fmm_main_action);
  _init_sources     = HPX_REGISTER_ACTION(_init_sources_action);
  _init_charges     = HPX_REGISTER_ACTION(_init_charges_action);
  _init_targets     = HPX_REGISTER_ACTION(_init_targets_action);
  _init_mapsrc      = HPX_REGISTER_ACTION(_init_mapsrc_action); 
  _init_maptar      = HPX_REGISTER_ACTION(_init_maptar_action); 
  _init_param       = HPX_REGISTER_ACTION(_init_param_action); 
  _init_source_root = HPX_REGISTER_ACTION(_init_source_root_action); 
  _init_target_root = HPX_REGISTER_ACTION(_init_target_root_action); 

  e = hpx_run(_fmm_main, NULL, 0); 
  if (e) {
    fprintf(stderr, "HPX: error while running.\n");
    return e;
  } 

  return 0;
}

