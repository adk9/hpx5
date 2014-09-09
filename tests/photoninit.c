#include <check.h>
#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>
#include "photon.h"
#include "test_cfg.h"
FILE * perf_log;

/*
 --------------------------------------------------------------------
  TEST SUITE FIXTURE: library initialization
 --------------------------------------------------------------------
*/
void photontest_core_setup(void) {
  int rank, size;
  /* open a performance log file */
  perf_log = fopen("perf.log", "w+");
  ck_assert_msg(perf_log != NULL, "Could not open performance log");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  cfg.nproc = size;
  cfg.address = rank;

  photon_init(&cfg);
}

/*
 --------------------------------------------------------------------
  TEST SUITE FIXTURE: library cleanup
 --------------------------------------------------------------------
*/
void photontest_core_teardown(void) {
  photon_finalize();
  fclose(perf_log);
}

