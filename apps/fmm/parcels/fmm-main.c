/// ---------------------------------------------------------------------------
/// @file fmm-main.c
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief  Main file for FMM
/// ---------------------------------------------------------------------------

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "hpx/hpx.h"
#include "fmm.h"

int nsources; ///< number of sources
int ntargets; ///< number of targets
int datatype; ///< distribution of the source and tagret ensembles
int accuracy; ///< accuracy of the requirement
int s;        ///< partition criterion

hpx_action_t _fmm_main;
hpx_action_t _init_sources;
hpx_action_t _init_targets;
hpx_action_t _init_param;
hpx_action_t _init_source_root;
hpx_action_t _init_target_root;
hpx_action_t _partition_box;
hpx_action_t _swap;
hpx_action_t _set_box;
hpx_action_t _aggregate;
hpx_action_t _source_to_mpole;
hpx_action_t _mpole_to_mpole;
hpx_action_t _mpole_to_expo;
hpx_action_t _disaggregate;
hpx_action_t _build_list5;
hpx_action_t _query_box;
hpx_action_t _source_to_local;
hpx_action_t _delete_box;
hpx_action_t _merge_expo;
hpx_action_t _merge_expo_zp;
hpx_action_t _merge_expo_zm;
hpx_action_t _merge_update;
hpx_action_t _shift_expo_c1;
hpx_action_t _shift_expo_c2;
hpx_action_t _shift_expo_c3;
hpx_action_t _shift_expo_c4;
hpx_action_t _shift_expo_c5;
hpx_action_t _shift_expo_c6;
hpx_action_t _shift_expo_c7;
hpx_action_t _shift_expo_c8;
hpx_action_t _merge_local;
hpx_action_t _local_to_local;
hpx_action_t _proc_target;
hpx_action_t _local_to_target;
hpx_action_t _proc_list1;
hpx_action_t _proc_list5;
hpx_action_t _source_to_target;
hpx_action_t _mpole_to_target;

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: fmm [options]\n"
      "\t-n, number of source points\n"
      "\t-m, number of target points\n"
      "\t-a, accuracy requirement, only 3 and 6 are valid\n"
      "\t-d, distribution of the particles\n"
      "\t    type-1 data is uniformly distributed inside a box\n"
      "\t    type-2 data is uniformly distributed over a sphere\n"
      "\t-s, maximum number of points allowed per leaf box\n"
      "\t-h, show help\n");
  hpx_print_help();
  fflush(stream);
}

int main(int argc, char *argv[]) {
  nsources = 10000;
  ntargets = 10000;
  datatype = 1;
  accuracy = 3;
  s        = 40;

  // Initialize hpx runtime
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX:failed to initialize.\n");
    return e;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "n:m:a:d:s:h?")) != -1) {
    switch (opt) {
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

  // Register actions
  _fmm_main         = HPX_REGISTER_ACTION(_fmm_main_action);
  _init_sources     = HPX_REGISTER_ACTION(_init_sources_action);
  _init_targets     = HPX_REGISTER_ACTION(_init_targets_action);
  _init_param       = HPX_REGISTER_ACTION(_init_param_action);
  _init_source_root = HPX_REGISTER_ACTION(_init_source_root_action);
  _init_target_root = HPX_REGISTER_ACTION(_init_target_root_action);
  _partition_box    = HPX_REGISTER_ACTION(_partition_box_action);
  _swap             = HPX_REGISTER_ACTION(_swap_action);
  _set_box          = HPX_REGISTER_ACTION(_set_box_action);
  _aggregate        = HPX_REGISTER_ACTION(_aggregate_action);
  _source_to_mpole  = HPX_REGISTER_ACTION(_source_to_multipole_action);
  _mpole_to_mpole   = HPX_REGISTER_ACTION(_multipole_to_multipole_action);
  _mpole_to_expo    = HPX_REGISTER_ACTION(_multipole_to_exponential_action);
  _disaggregate     = HPX_REGISTER_ACTION(_disaggregate_action);
  _build_list5      = HPX_REGISTER_ACTION(_build_list5_action);
  _query_box        = HPX_REGISTER_ACTION(_query_box_action);
  _source_to_local  = HPX_REGISTER_ACTION(_source_to_local_action);
  _delete_box       = HPX_REGISTER_ACTION(_delete_box_action);
  _merge_expo       = HPX_REGISTER_ACTION(_merge_exponential_action);
  _merge_expo_zp    = HPX_REGISTER_ACTION(_merge_exponential_zp_action);
  _merge_expo_zm    = HPX_REGISTER_ACTION(_merge_exponential_zm_action);
  _merge_update     = HPX_REGISTER_ACTION(_merge_update_action);
  _shift_expo_c1    = HPX_REGISTER_ACTION(_shift_exponential_c1_action);
  _shift_expo_c2    = HPX_REGISTER_ACTION(_shift_exponential_c2_action);
  _shift_expo_c3    = HPX_REGISTER_ACTION(_shift_exponential_c3_action);
  _shift_expo_c4    = HPX_REGISTER_ACTION(_shift_exponential_c4_action);
  _shift_expo_c5    = HPX_REGISTER_ACTION(_shift_exponential_c5_action);
  _shift_expo_c6    = HPX_REGISTER_ACTION(_shift_exponential_c6_action);
  _shift_expo_c7    = HPX_REGISTER_ACTION(_shift_exponential_c7_action);
  _shift_expo_c8    = HPX_REGISTER_ACTION(_shift_exponential_c8_action);
  _merge_local      = HPX_REGISTER_ACTION(_merge_local_action);
  _local_to_local   = HPX_REGISTER_ACTION(_local_to_local_action);
  _proc_target      = HPX_REGISTER_ACTION(_proc_target_action);
  _local_to_target  = HPX_REGISTER_ACTION(_local_to_target_action);
  _proc_list1       = HPX_REGISTER_ACTION(_proc_list1_action);
  _proc_list5       = HPX_REGISTER_ACTION(_proc_list5_action);
  _source_to_target = HPX_REGISTER_ACTION(_source_to_target_action);
  _mpole_to_target  = HPX_REGISTER_ACTION(_multipole_to_target_action);

  e = hpx_run(_fmm_main, NULL, 0);
  if (e) {
    fprintf(stderr, "HPX: error while running.\n");
    return e;
  }

  return 0;
}

