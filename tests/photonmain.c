#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <check.h>
#include "tests.h"

int main(int argc, char *argv[]) {
  Suite * s = suite_create("photontest");
  TCase * tc = tcase_create("photontest-core");

  MPI_Init(&argc,&argv);

  tcase_add_unchecked_fixture(tc, photontest_core_setup, photontest_core_teardown);
  /* set timeout */
  tcase_set_timeout(tc, 1200);

  add_photon_test(tc);  /*photon_test.c*/ 
  add_photon_data_movement(tc); // Photon get and put tests
  add_photon_message_passing(tc); // photon forwarder and interleaved
  add_photon_pingpong(tc); // photon pingpong test
  add_photon_comm_test(tc);
  add_photon_buffers_remote_test(tc);
  add_photon_buffers_private_test(tc);
  add_photon_send_request_test(tc);
  add_photon_rdma_one_sided(tc);

  suite_add_tcase(s, tc);

  SRunner * sr = srunner_create(s);

  //Outputs the result to test.log
  srunner_set_log(sr, "test.log");

  // This sets CK_FORK=no
  srunner_set_fork_status(sr, CK_NOFORK);

  srunner_run_all(sr, CK_NORMAL);

  int failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  
  MPI_Finalize();

  return (failed == 0) ? 0 : -1;
}
