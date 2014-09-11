#ifndef PHOTON_TESTS_TESTS_H_
#define PHOTON_TESTS_TESTS_H_

#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>

void photontest_core_setup(void);
void photontest_core_teardown(void);

void add_photon_test(TCase *);
void add_photon_data_movement(TCase *);
void add_photon_message_passing(TCase *);
void add_photon_pingpong(TCase *);
void add_photon_comm_test(TCase *);
void add_photon_buffers_remote_test(TCase *);
void add_photon_buffers_private_test(TCase *);
void add_photon_send_request_test(TCase *);

#endif /*PHOTON_TESTS_TESTS_H_*/
