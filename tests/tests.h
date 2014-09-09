#ifndef PHOTON_TESTS_TESTS_H_
#define PHOTON_TESTS_TESTS_H_

#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>

void photontest_core_setup(void);
void photontest_core_teardown(void);

void add_photon_test(TCase *);

#endif /*PHOTON_TESTS_TESTS_H_*/
