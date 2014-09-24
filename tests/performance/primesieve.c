//****************************************************************************
// @Project Includes
//****************************************************************************
#include "hpx/hpx.h"
#include "tests.h"
#include <stdlib.h>

#define LIMIT 10000000 /*size of integers array*/

//****************************************************************************
// Test code -- for GAS local memory allocation
//****************************************************************************
START_TEST (test_primesieve)
{
  printf("Starting the prime sieve test\n");

} 
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************
void add_primesieve(TCase *tc) {
  tcase_add_test(tc, test_primesieve);
}
