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

static int _sender_action(hpx_addr_t *channel) {
  uint64_t data = 1234;
  // Send the number
  printf("Source sending data = %" PRIu64 "\n", data);
  hpx_addr_t rfut = hpx_lco_future_new(0);
  // Channel - send. 
  hpx_lco_chan_send(channel[0], sizeof(data), &data, rfut, HPX_NULL);
  hpx_lco_wait(rfut);
  hpx_lco_delete(rfut, HPX_NULL);
  return HPX_SUCCESS;
}

//****************************************************************************
// Testcase to test hpx_lco_chan_try_recv function, which probes a single 
// channel to attempt to read. The hpx_lco_chan_try_recv() operation will 
// return HPX_LCO_CHAN_EMPTY to indicate that no buffer was available.
//****************************************************************************
static int _receiver_action(hpx_addr_t *channel){
  // Probe for an incoming message.
  uint64_t *result;
  int size = sizeof(uint64_t);

  hpx_status_t status = hpx_lco_chan_try_recv(channel[0], &size , (void**)&result);
  if (status == HPX_LCO_CHAN_EMPTY) {
    hpx_lco_chan_recv(channel[0], &size , (void**)&result);
  }
 
  printf("The data received by destination is: = %" PRIu64 "\n", *result);

  return HPX_SUCCESS;
}

static int _main_action(void *args) {
  hpx_addr_t chan[1] = {
    hpx_lco_chan_new()
  };
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

