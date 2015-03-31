#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <check.h>
#include <assert.h>
#include "tests.h"

int64_t getMicrosecondTimeStamp()
{
  int64_t retval;
  struct timeval tv;
  if (gettimeofday(&tv, NULL)) {
    perror("gettimeofday");
    abort();
  }
  retval = ((int64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
  return retval;
}

int main(int argc, char *argv[]) {
  Suite * s = suite_create("photontest");
  TCase * tc = tcase_create("photontest-core");

  MPI_Init(&argc,&argv);

  tcase_add_unchecked_fixture(tc, photontest_core_setup, photontest_core_teardown);
  /* set timeout */
  tcase_set_timeout(tc, 1200);
  add_photon_pingpong(tc);                // BEGIN tests that care about PWC payload
  add_photon_rdma_with_completion(tc);
  add_photon_buffers_remote_test(tc);
  add_photon_buffers_private_test(tc);
  //add_photon_put_wc(tc);                  // END tests that care about PWC payload
  add_photon_test(tc);                    // photon_test.c
  add_photon_data_movement(tc);           // Photon get and put tests
  add_photon_message_passing(tc);         // photon interleaved
  add_photon_send_request_test(tc);
  add_photon_rdma_one_sided_put(tc);
  add_photon_rdma_one_sided_get(tc);
  add_photon_threaded_put_wc(tc);
  add_photon_os_get_bench(tc);
  add_photon_os_put_bench(tc);
  //add_photon_send_buffer_bench(tc);
  //add_photon_recv_buffer_bench(tc);
  add_photon_send_buffer_bd_bench(tc);
  add_photon_recv_buffer_bd_bench(tc);
  //add_photon_put_wc_bw_bench(tc);
  suite_add_tcase(s, tc);

  SRunner * sr = srunner_create(s);

  // This sets CK_FORK=no
  srunner_set_fork_status(sr, CK_NOFORK);

  srunner_run_all(sr, CK_VERBOSE);

  int failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  
  MPI_Finalize();

  return (failed == 0) ? 0 : -1;
}
