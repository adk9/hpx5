#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <stdlib.h>
#include <unistd.h>
#include <check.h>
#include "tests.h"
#include "photon.h"

#define PHOTON_SEND_SIZE (1024*16)

//****************************************************************************
// This unit testcase tests photon buffers functions:
// 1. photon_register_buffer
// 2. photon_unregister_buffer
// 3. photon_get_buffer_private
//****************************************************************************


START_TEST (test_photon_get_private_buffers) 
{
  void *mybuf = malloc(PHOTON_SEND_SIZE*sizeof(char));
  int rc;
  struct photon_buffer_priv_t rpriv = {
    .key0 = UINT64_MAX,
    .key1 = UINT64_MAX
  };
  const struct photon_buffer_priv_t *priv = &rpriv;

  fprintf(detailed_log,"Starting the photon get private buffer test\n");
  
  rc = photon_get_buffer_private(mybuf, PHOTON_SEND_SIZE, &priv);
  ck_assert_msg(rc == PHOTON_ERROR, "photon_get_buffer_private before register returned wrong output");

  rc = photon_register_buffer(mybuf, PHOTON_SEND_SIZE);
  ck_assert_msg(rc == PHOTON_OK, "photon_register_buffer failed");

  rc = photon_get_buffer_private(mybuf, PHOTON_SEND_SIZE, &priv);
  ck_assert_msg(rc == PHOTON_OK, "photon_get_buffer_private after register buffer failed");
  ck_assert_msg(priv->key0 != UINT64_MAX, "key0 is not set");
  ck_assert_msg(priv->key1 != UINT64_MAX, "key1 is not set");
  
  rc = photon_get_buffer_private(mybuf+1024, PHOTON_SEND_SIZE-2048, &priv);
  ck_assert_msg(rc == PHOTON_OK, "photon_get_buffer_private with offset after register buffer failed");
  ck_assert_msg(priv->key0 != UINT64_MAX, "key0 is not set");
  ck_assert_msg(priv->key1 != UINT64_MAX, "key1 is not set");
  
  photon_unregister_buffer(mybuf, PHOTON_SEND_SIZE);
  free(mybuf);
}
END_TEST

//****************************************************************************
// Register the testcase photon_buffer.c
//****************************************************************************
void add_photon_buffers_private_test(TCase *tc) {
  tcase_add_test(tc, test_photon_get_private_buffers);
}
