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

hpx_action_t _fmm_main; 
hpx_action_t _init_param; 
hpx_action_t _partition_box; 

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

  fmm_config_t fmm_cfg = {
    .nsources = 10000,
    .ntargets = 10000,
    .datatype = 1,
    .accuracy = 3,
    .s        = 40
  };

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
      fmm_cfg.nsources = atoi(optarg);
      break;
    case 'm':
      fmm_cfg.ntargets = atoi(optarg);
      break;
    case 'a':
      fmm_cfg.accuracy = atoi(optarg);
      break;
    case 'd':
      fmm_cfg.datatype = atoi(optarg);
      break;
    case 's':
      fmm_cfg.s        = atoi(optarg);
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
  _fmm_main             = HPX_REGISTER_ACTION(_fmm_main_action);
  _init_param           = HPX_REGISTER_ACTION(_init_param_action); 
  _partition_box        = HPX_REGISTER_ACTION(_partition_box_action); 

  e = hpx_run(_fmm_main, &fmm_cfg, sizeof(fmm_cfg)); 
  if (e) {
    fprintf(stderr, "HPX: error while running.\n");
    return e;
  } 

  

  /*
  // register actions
  _fmm_build_list134    = HPX_REGISTER_ACTION(_fmm_build_list134_action); 
  _fmm_bcast            = HPX_REGISTER_ACTION(_fmm_bcast_action); 
  _aggr_leaf_sbox       = HPX_REGISTER_ACTION(_aggr_leaf_sbox_action);
  _aggr_nonleaf_sbox    = HPX_REGISTER_ACTION(_aggr_nonleaf_sbox_action);
  _disaggr_leaf_tbox    = HPX_REGISTER_ACTION(_disaggr_leaf_tbox_action);
  _disaggr_nonleaf_tbox = HPX_REGISTER_ACTION(_disaggr_nonleaf_tbox_action);
  _recv_result          = HPX_REGISTER_ACTION(_recv_result_action);
  _process_near_field   = HPX_REGISTER_ACTION(_process_near_field_action);

  // run the main action
  fmm_param = construct_param(fmm_cfg.accuracy);
  if (debug == hpx_get_my_rank()) {
    int i = 0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("PID %d on %s ready for attach\n", getpid(), hostname);
    fflush(stdout);
    while (0 == i)
      sleep(12);
  }

  e = hpx_run(_fmm_main, &fmm_cfg, sizeof(fmm_cfg));
  if (e) {
    fprintf(stderr, "HPX: error while running.\n");
    return e;
  }

  // cleanup
  destruct_param(&fmm_param);
  */
  return 0;
}

