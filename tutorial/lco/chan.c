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

int number_amount;

static int _sender_action(hpx_addr_t *channel) {
  const int MAX_NUMBERS = 100;
  int numbers[MAX_NUMBERS];

  srand(time(NULL));
  number_amount = (rand() / (float)RAND_MAX) * MAX_NUMBERS;

  printf("Sending the data: ");
  for (int i = 0; i < number_amount; i++){
    numbers[i] = i;
    printf(" %d ", numbers[i]);
  }
  printf("\n");

  hpx_addr_t rfut = hpx_lco_future_new(0);
  // Channel - send. 
  hpx_lco_chan_send(channel[0], number_amount * sizeof(int), numbers, rfut, HPX_NULL);
  printf("Sent %d numbers\n", number_amount);
  hpx_lco_wait(rfut);
  hpx_lco_delete(rfut, HPX_NULL);
  return HPX_SUCCESS;
}

static int _receiver_action(hpx_addr_t *channel){
  int size = sizeof(int) * number_amount;
  // Allocate a buffer just big enough to hold the incoming numbers
  void *number_buf = (void*) malloc(size);
  // Now receive the message with the allocated buffer
  hpx_lco_chan_recv(channel[0], &size, &number_buf);

  printf("Received the data: ");
  for (int i = 0; i < number_amount; i++) {
    printf(" %d ", ((int *) number_buf)[i]);
  }
  printf("\n");
  
  free(number_buf);
  return HPX_SUCCESS;
} 

static int _main_action(void *args) {
  hpx_addr_t chan[1] = {hpx_lco_chan_new()};
  hpx_addr_t done = hpx_lco_and_new(2);
  hpx_call(HPX_HERE, _sender, chan, sizeof(chan), done);
  hpx_call(HPX_HERE, _receiver, chan, sizeof(chan), done);
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
   
  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_sender, _sender_action);
  HPX_REGISTER_ACTION(&_receiver, _receiver_action);
  return hpx_run(&_main, NULL, 0);
}
