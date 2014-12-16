// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <hpx/hpx.h>

static hpx_action_t _main       = 0;
static hpx_action_t _sender     = 0;
static hpx_action_t _receiver   = 0;

static int _sender_action(hpx_addr_t *args) {
  uint64_t *result;
  uint64_t data = 1234;
  hpx_addr_t channels = *args;

  printf("Source sending data = %"PRIu64"\n", data);
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_addr_t addr = hpx_lco_chan_array_at(channels, 0, 1, 1);
  hpx_lco_chan_send(addr, sizeof(data), &data, done, HPX_NULL);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  hpx_addr_t chan = hpx_lco_chan_array_at(channels, 1, 1, 1);
  hpx_lco_chan_recv(chan, NULL, (void**)&result);
  printf("The data received by source is: = %"PRIu64"\n", *result);
  free(result);
  return HPX_SUCCESS;
}

int _receiver_action(hpx_addr_t *args) {
  uint64_t *result;
  uint64_t data = 5678;
  hpx_addr_t channels = *args;
  hpx_addr_t addr = hpx_lco_chan_array_at(channels, 0, 1, 1);
  hpx_lco_chan_recv(addr, NULL, (void**)&result);
  printf("The data received by destination is: = %"PRIu64"\n", *result);
  free(result);

  hpx_addr_t done = hpx_lco_future_new(0);
  printf("Destination sending data = %"PRIu64"\n", data);
  addr = hpx_lco_chan_array_at(channels, 1, 1, 1);
  hpx_lco_chan_send(addr, sizeof(data), &data, done, HPX_NULL);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  return HPX_SUCCESS;
}

static int _main_action(void *args) {
  hpx_addr_t addr1, addr2;
  // Allocate a global array of 2 channels with block size of 1.
  hpx_addr_t channels = hpx_lco_chan_array_new(2, 1, 1);
  hpx_addr_t completed = hpx_lco_and_new(2);

  addr1 = hpx_lco_chan_array_at(channels, 0, 1, 1);
  hpx_call(addr1, _sender, &channels, sizeof(channels), completed);
  addr2 = hpx_lco_chan_array_at(channels, 1, 1, 1);
  hpx_call(addr2, _receiver, &channels, sizeof(channels), completed);

  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);
  hpx_lco_chan_array_delete(channels, HPX_NULL);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_sender, _sender_action);
  HPX_REGISTER_ACTION(&_receiver, _receiver_action);
  return hpx_run(&_main, NULL, 0);
}
