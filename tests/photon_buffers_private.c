#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <stdlib.h>
#include <unistd.h>
#include <check.h>
#include "tests.h"
#include "photon.h"

#define PHOTON_SEND_SIZE 32

//****************************************************************************
// This unit testcase tests photon buffers functions:
// 1. photon_register_buffer
// 2. photon_unregister_buffer
// 3. photon_get_buffer_private
//****************************************************************************


START_TEST (test_photon_get_private_buffers) 
{
  struct photon_buffer_t *desc;
  int rc;
  printf("Starting the photon get private buffer test\n");
  photonBufferPriv ret = malloc(sizeof(struct photon_buffer_priv_t));
  ret->key0 = 2576896;
  ret->key1 = 2576896;

  desc = malloc(PHOTON_SEND_SIZE * sizeof(struct photon_buffer_t));
  rc = photon_get_buffer_private(&desc, PHOTON_SEND_SIZE, ret);
  ck_assert_msg(rc == PHOTON_ERROR, "photon_get_buffer_private before register returned wrong output");

  photon_register_buffer(desc, PHOTON_SEND_SIZE);
  rc = photon_get_buffer_private(&desc, PHOTON_SEND_SIZE, ret);
  ck_assert_msg(rc == PHOTON_OK, "photon_get_buffer_private after register buffer failed"); 
 
  photon_unregister_buffer(desc, PHOTON_SEND_SIZE);
  free(ret);
  free(desc);
}
END_TEST

//****************************************************************************
// Register the testcase photon_buffer.c
//****************************************************************************
void add_photon_buffers_private_test(TCase *tc) {
  tcase_add_test(tc, test_photon_get_private_buffers);
}
