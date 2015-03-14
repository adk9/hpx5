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

// Goal of this testcase is to test the HPX LCO Channels
//  1. hpx_lco_chan_new -- Allocate a new channel.
//  2. hpx_lco_chan_recv -- Receive a buffer from a channel.
//  3. hpx_lco_chan_send -- Send a buffer through a channel. 
//  4. hpx_lco_chan_send_inorder -- Send a buffer through an
//                                  ordered channel.
//  5. hpx_lco_chan_try_recv -- Probe a single channel to 
//                              attempt to read.
//  6. hpx_lco_chan_array_new -- Allocate a global array of
//                               channels.
//  7. hpx_lco_chan_array_select -- Receive from one of a set of
//                               channels.
//  8. hpx_lco_chan_array_at
//  9. hpx_lco_chan_array_delete

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hpx/hpx.h"
#include "tests.h"

// Test code -- for HPX LCO Channels. This testcase tests functions,
// hpx_lco_chan_new, hpx_lco_chan_send, hpx_lco_chan_recv functions.
static HPX_ACTION(_sender, hpx_addr_t *channels) {
  uint64_t data = 1234;
  
  hpx_thread_set_affinity(0);

  //printf("Source sending data = %"PRIu64"\n", data);
  hpx_addr_t done = hpx_lco_future_new(0);
  // Channel send -- Send a buffer through a channel.
  hpx_lco_chan_send(channels[0], sizeof(data), &data, done, HPX_NULL);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  void *rbuf;
  // Receive a buffer from a channel. This is a blocking call.
  hpx_status_t status = hpx_lco_chan_recv(channels[1], NULL, &rbuf);
  assert_msg(status == HPX_SUCCESS, "LCO Channel receive failed");
  //uint64_t result = *(uint64_t*)rbuf;
  // free the buffer that it receives on the channel.
  free(rbuf);
  //printf("The data received by source is: = %"PRIu64"\n", result);

  return HPX_SUCCESS;
}

static HPX_ACTION(_receiver, hpx_addr_t *channels) {
  uint64_t data = 5678;
  
  hpx_thread_set_affinity(1);

  void *rbuf;
  hpx_lco_chan_recv(channels[0], NULL, &rbuf);
  //uint64_t result = *(uint64_t*)rbuf;
  free(rbuf);
  //printf("The data received by destination is: = %"PRIu64"\n", result);

  hpx_addr_t done = hpx_lco_future_new(0);
  //printf("Destination sending data = %"PRIu64"\n", data);
  hpx_lco_chan_send(channels[1], sizeof(data), &data, done, HPX_NULL);
  hpx_lco_wait(done); 
  hpx_lco_delete(done, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_channel_send_recv, void *UNUSED) {
  printf("Starting the HPX LCO Channels test\n");
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(2);
  // The channel LCO approximates an MPI Channel. This allocates a new channel.
  // addr is the global address of the newly allocated channel.
 
  hpx_addr_t channels[2] = {
    hpx_lco_chan_new(),
    hpx_lco_chan_new()
  }; 

  // Spawn the sender thread.
  hpx_call(HPX_HERE, _sender, done, channels, sizeof(channels)); 
  hpx_call(HPX_HERE, _receiver, done, channels, sizeof(channels));
  hpx_lco_wait(done);

  hpx_lco_delete(channels[0], HPX_NULL);
  hpx_lco_delete(channels[1], HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
} 

// Send a buffer through an ordered channel. All in order sends from a thread
// are guaranteed to be received in the order that they were sent. No
// guarantee is made for inorder sends on the same channel from different
// threads. 
#define PING_PONG_LIMIT 5

static HPX_ACTION(_send_in_order, hpx_addr_t *chans) {
  int count = 0;
  while (count < PING_PONG_LIMIT) {
    //printf("Source sending data = %d\n", count);
    hpx_addr_t done = hpx_lco_future_new(0);
    // Send a buffer in order through a channel.
    hpx_lco_chan_send_inorder(chans[0], sizeof(int), &count, done);
    hpx_lco_wait(done);
    hpx_lco_delete(done, HPX_NULL);

    void *rbuf;
    hpx_lco_chan_recv(chans[1], NULL, &rbuf);
    free(rbuf);
   
    count++;
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(_receive_in_order, hpx_addr_t *chans) {
  int count = 0;
  while (count < PING_PONG_LIMIT) {
    void *rbuf;
    hpx_lco_chan_recv(chans[0], NULL, &rbuf);
    int result = *(int*)rbuf;
    free(rbuf);
    //printf("The data received by destination is: = %d\n", result);
    assert(result == count);

    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_lco_chan_send_inorder(chans[1], sizeof(int), &count, done);
    hpx_lco_wait(done);
    hpx_lco_delete(done, HPX_NULL);
    count++;
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_channel_send_in_order, void *UNUSED) {
  printf("Starting the HPX LCO Channels In order send test\n");
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(2);
  hpx_addr_t channel[2] = {
    hpx_lco_chan_new(),
    hpx_lco_chan_new()
  };

  hpx_call(HPX_HERE, _send_in_order, done, channel, sizeof(channel));
  hpx_call(HPX_HERE, _receive_in_order, done, channel, sizeof(channel));
  hpx_lco_wait(done);

  hpx_lco_delete(channel[0], HPX_NULL);
  hpx_lco_delete(channel[1], HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

// Testcase to test hpx_lco_chan_try_recv function, which probes a single 
// channel to attempt to read. The hpx_lco_chan_try_recv() operation will 
// return HPX_LCO_CHAN_EMPTY to indicate that no buffer was available.
static HPX_ACTION(_tryrecvempty, hpx_addr_t *args) {
  void *rbuf;
  hpx_addr_t addr = *args;
  hpx_status_t status = hpx_lco_chan_try_recv(addr, NULL, &rbuf);
  assert(status == HPX_LCO_CHAN_EMPTY);
  if (status == HPX_SUCCESS)
    free(rbuf);
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_channel_tryrecvempty, void *UNUSED) {
  printf("Starting the HPX LCO Channel Try Receive empty buffer test\n");
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(1);
  hpx_addr_t channel = hpx_lco_chan_new();
  hpx_call(HPX_HERE, _tryrecvempty, done, &channel, sizeof(channel));
  hpx_lco_wait(done);
  hpx_lco_delete(channel, HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

// This testcase tests the functions, hpx_lco_array_new and hpx_lco_chan_array
// _at functions == Allocate a global array of channels, and receive from one
// of a set of channels. 
static HPX_ACTION(_senderchannel, hpx_addr_t *args) {
  uint64_t *result;
  uint64_t data = 1234;
  hpx_addr_t channels = *args;

  //printf("Source sending data = %"PRIu64"\n", data);
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_addr_t addr = hpx_lco_chan_array_at(channels, 0, 8, 1);
  hpx_lco_chan_send(addr, sizeof(data), &data, done, HPX_NULL);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  hpx_addr_t chan = hpx_lco_chan_array_at(channels, 1, 8, 1);
  hpx_lco_chan_recv(chan, NULL, (void**)&result);
  //printf("The data received by source is: = %"PRIu64"\n", *result);
  free(result);
  return HPX_SUCCESS;
}

static HPX_ACTION(_receiverchannel, hpx_addr_t *args) {
  uint64_t *result;
  uint64_t data = 5678;
  hpx_addr_t channels = *args;
  hpx_addr_t addr = hpx_lco_chan_array_at(channels, 0, 8, 1);
  hpx_lco_chan_recv(addr, NULL, (void**)&result);
  //printf("The data received by destination is: = %"PRIu64"\n", *result);
  free(result);

  hpx_addr_t done = hpx_lco_future_new(0);
  //printf("Destination sending data = %"PRIu64"\n", data);
  addr = hpx_lco_chan_array_at(channels, 1, 8, 1);
  hpx_lco_chan_send(addr, sizeof(data), &data, done, HPX_NULL);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_channel_array, void *UNUSED) {
  printf("Starting the HPX LCO global array of channels test\n");
  hpx_addr_t addr1, addr2;  

  hpx_time_t t1 = hpx_time_now();

  // Allocate a global array of 2 channels with block size of 1.
  hpx_addr_t channels = hpx_lco_chan_array_new(2, 8, 1);
  hpx_addr_t completed = hpx_lco_and_new(2);
    
  addr1 = hpx_lco_chan_array_at(channels, 0, 8, 1);
  hpx_call(addr1, _senderchannel, completed, &channels, sizeof(channels));
  addr2 = hpx_lco_chan_array_at(channels, 1, 8, 1);
  hpx_call(addr2, _receiverchannel, completed, &channels, sizeof(channels));

  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);
  hpx_lco_chan_array_delete(channels, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

TEST_MAIN({
 ADD_TEST(lco_channel_send_recv);
 ADD_TEST(lco_channel_send_in_order);
 ADD_TEST(lco_channel_tryrecvempty);
 ADD_TEST(lco_channel_array);
});
