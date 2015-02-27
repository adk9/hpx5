// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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

#define LIMIT 5

static int _sender_action(hpx_addr_t *channel) {
  int count = 0;
  while (count < LIMIT) {
    printf("Source sending data = %d\n", count);
    hpx_addr_t done = hpx_lco_future_new(0);
    // Send a buffer in order through a channel.
    hpx_lco_chan_send_inorder(channel[0], sizeof(int), &count, done);
    hpx_lco_wait(done);
    hpx_lco_delete(done, HPX_NULL);
    count++;
  }
  return HPX_SUCCESS;
}

static int _receiver_action(hpx_addr_t *channel){
  int count = 0;
  while (count < LIMIT) {
    void *rbuf;
    hpx_lco_chan_recv(channel[0], NULL, &rbuf);
    int result = *(int*)rbuf;
    free(rbuf);
    printf("The data received by destination is: = %d\n", result);
    assert(result == count);
    count++;
  }
  return HPX_SUCCESS;
} 

static int _main_action(void *args) {
  hpx_addr_t chan[1] = {hpx_lco_chan_new()};
  hpx_addr_t done = hpx_lco_and_new(2);
  hpx_call(HPX_HERE, _sender, done, chan, sizeof(chan));
  hpx_call(HPX_HERE, _receiver, done, chan, sizeof(chan));
  hpx_lco_wait(done);

  hpx_lco_delete(chan[0], HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_sender_action, &_sender);
  HPX_REGISTER_ACTION(_receiver_action, &_receiver);
  return hpx_run(&_main, NULL, 0);
}
