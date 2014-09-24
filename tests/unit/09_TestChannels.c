//****************************************************************************
// @Filename      09_TestChan.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Channels
// 
// @Compiler      GCC
// @OS            Linux
// @Description   chan.c File Reference
// @Goal          Goal of this testcase is to test the HPX LCO Channels
//                1. hpx_lco_chan_new -- Allocate a new channel.
//                2. hpx_lco_chan_recv -- Receive a buffer from a channel.
//                3. hpx_lco_chan_send -- Send a buffer through a channel. 
//                4. hpx_lco_chan_send_inorder -- Send a buffer through an
//                                                ordered channel.
//                5. hpx_lco_chan_try_recv -- Probe a single channel to 
//                                            attempt to read.
//                6. hpx_lco_chan_array_new -- Allocate a global array of
//                                             channels.
//                7. hpx_lco_chan_array_select -- Receive from one of a set of
//                                             channels.
//                8. hpx_lco_chan_array_at
//                9. hpx_lco_chan_array_delete
// @Copyright     Copyright (c) 2014, Trustees of Indiana University
//                All rights reserved.
//
//                This software may be modified and distributed under the terms
//                of the BSD license.  See the COPYING file for details.
//
//                This software was created at the Indiana University Center 
//                for Research in Extreme Scale Technologies (CREST).
//----------------------------------------------------------------------------
// @Date          09/04/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hpx/hpx.h"
#include "tests.h"

//****************************************************************************
// Test code -- for HPX LCO Channels. This testcase tests functions,
// hpx_lco_chan_new, hpx_lco_chan_send, hpx_lco_chan_recv functions.
//****************************************************************************
int t09_sender_action(hpx_addr_t *channels) {
  uint64_t data = 1234;
  
  hpx_thread_set_affinity(0);

  printf("Source sending data = %"PRIu64"\n", data);
  hpx_addr_t done = hpx_lco_future_new(0);
  // Channel send -- Send a buffer through a channel.
  hpx_lco_chan_send(channels[0], sizeof(data), &data, done, HPX_NULL);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  void *rbuf;
  // Receive a buffer from a channel. This is a blocking call.
  hpx_status_t status = hpx_lco_chan_recv(channels[1], NULL, &rbuf);
  ck_assert_msg(status == HPX_SUCCESS, "LCO Channel receive failed");
  uint64_t result = *(uint64_t*)rbuf;
  // free the buffer that it receives on the channel.
  free(rbuf);
  printf("The data received by source is: = %"PRIu64"\n", result);

  return HPX_SUCCESS;
}

int t09_receiver_action(hpx_addr_t *channels) {
  uint64_t data = 5678;
  
  hpx_thread_set_affinity(1);

  void *rbuf;
  hpx_lco_chan_recv(channels[0], NULL, &rbuf);
  uint64_t result = *(uint64_t*)rbuf;
  free(rbuf);
  printf("The data received by destination is: = %"PRIu64"\n", result);

  hpx_addr_t done = hpx_lco_future_new(0);
  printf("Destination sending data = %"PRIu64"\n", data);
  hpx_lco_chan_send(channels[1], sizeof(data), &data, done, HPX_NULL);
  hpx_lco_wait(done); 
  hpx_lco_delete(done, HPX_NULL);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_lco_channelSendRecv)
{
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
  hpx_call(HPX_HERE, t09_sender, channels, sizeof(channels), done); 
  hpx_call(HPX_HERE, t09_receiver, channels, sizeof(channels), done);
  hpx_lco_wait(done);

  hpx_lco_delete(channels[0], HPX_NULL);
  hpx_lco_delete(channels[1], HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST

//****************************************************************************
// Send a buffer through an ordered channel. All in order sends from a thread
// are guaranteed to be received in the order that they were sent. No
// guarantee is made for inorder sends on the same channel from different
// threads. 
//****************************************************************************
#define PING_PONG_LIMIT 5

int t09_sendInOrder_action(hpx_addr_t *chans) {
  int count = 0;
  while (count < PING_PONG_LIMIT) {
    printf("Source sending data = %d\n", count);
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

int t09_receiveInOrder_action(hpx_addr_t *chans) {
  int count = 0;
  while (count < PING_PONG_LIMIT) {
    void *rbuf;
    hpx_lco_chan_recv(chans[0], NULL, &rbuf);
    int result = *(int*)rbuf;
    free(rbuf);
    printf("The data received by destination is: = %d\n", result);
    ck_assert(result == count);

    hpx_addr_t done = hpx_lco_future_new(0);
    hpx_lco_chan_send_inorder(chans[1], sizeof(int), &count, done);
    hpx_lco_wait(done);
    hpx_lco_delete(done, HPX_NULL);
    count++;
  }
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_lco_channelSendInOrder) 
{
  printf("Starting the HPX LCO Channels In order send test\n");
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(2);
  hpx_addr_t channel[2] = {
    hpx_lco_chan_new(),
    hpx_lco_chan_new()
  };

  hpx_call(HPX_HERE, t09_sendInOrder, channel, sizeof(channel), done);
  hpx_call(HPX_HERE, t09_receiveInOrder, channel, sizeof(channel), done);
  hpx_lco_wait(done);

  hpx_lco_delete(channel[0], HPX_NULL);
  hpx_lco_delete(channel[1], HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Testcase to test hpx_lco_chan_try_recv function, which probes a single 
// channel to attempt to read. The hpx_lco_chan_try_recv() operation will 
// return HPX_LCO_CHAN_EMPTY to indicate that no buffer was available.
//****************************************************************************
int t09_tryRecvEmpty_action(void *args) {
  void *rbuf;
  hpx_addr_t addr = *(hpx_addr_t*)args;
  hpx_status_t status = hpx_lco_chan_try_recv(addr, NULL, &rbuf);
  ck_assert(status == HPX_LCO_CHAN_EMPTY);
  if (status == HPX_SUCCESS)
    free(rbuf);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_lco_channelTryRecvEmpty) 
{
  printf("Starting the HPX LCO Channel Try Receive empty buffer test\n");
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t done = hpx_lco_and_new(1);
  hpx_addr_t channel = hpx_lco_chan_new();
  hpx_call(HPX_HERE, t09_tryRecvEmpty, &channel, sizeof(channel), done);
  hpx_lco_wait(done);
  hpx_lco_delete(channel, HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// This testcase tests the functions, hpx_lco_array_new and hpx_lco_chan_array
// _at functions == Allocate a global array of channels, and receive from one
// of a set of channels. 
//****************************************************************************
int t09_senderChannel_action(hpx_addr_t *args) {
  uint64_t *result;
  uint64_t data = 1234;
  hpx_addr_t channels = *args;

  printf("Source sending data = %"PRIu64"\n", data);
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_addr_t addr = hpx_lco_chan_array_at(channels, 0);
  hpx_lco_chan_send(addr, sizeof(data), &data, done, HPX_NULL);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  hpx_addr_t chan = hpx_lco_chan_array_at(channels, 1);
  hpx_lco_chan_recv(chan, NULL, (void**)&result);
  printf("The data received by source is: = %"PRIu64"\n", *result);
  free(result);
  return HPX_SUCCESS;
}

int t09_receiverChannel_action(hpx_addr_t *args) {
  uint64_t *result;
  uint64_t data = 5678;
  hpx_addr_t channels = *args;
  hpx_addr_t addr = hpx_lco_chan_array_at(channels, 0);
  hpx_lco_chan_recv(addr, NULL, (void**)&result);
  printf("The data received by destination is: = %"PRIu64"\n", *result);
  free(result);

  hpx_addr_t done = hpx_lco_future_new(0);
  printf("Destination sending data = %"PRIu64"\n", data);
  addr = hpx_lco_chan_array_at(channels, 1);
  hpx_lco_chan_send(addr, sizeof(data), &data, done, HPX_NULL);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_lco_channelArray) 
{
  printf("Starting the HPX LCO global array of channels test\n");
  hpx_addr_t addr1, addr2;  

  hpx_time_t t1 = hpx_time_now();

  // Allocate a global array of 2 channels with block size of 1.
  hpx_addr_t channels = hpx_lco_chan_array_new(2, 1);
  hpx_addr_t completed = hpx_lco_and_new(2);
    
  addr1 = hpx_lco_chan_array_at(channels, 0);
  hpx_call(addr1, t09_senderChannel, &channels, sizeof(channels), completed);
  addr2 = hpx_lco_chan_array_at(channels, 1);
  hpx_call(addr2, t09_receiverChannel, &channels, sizeof(channels), completed);

  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);
  hpx_lco_chan_array_delete(channels, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST
//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_09_TestChannels(TCase *tc) {
  tcase_add_test(tc, test_libhpx_lco_channelSendRecv);
  tcase_add_test(tc, test_libhpx_lco_channelSendInOrder);
  tcase_add_test(tc, test_libhpx_lco_channelTryRecvEmpty);
  tcase_add_test(tc, test_libhpx_lco_channelArray);
}
