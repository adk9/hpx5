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

//****************************************************************************
// Example code - A hello world program which demonstrates another safe way
// to pass arguments to threads during thread creation. In this case, a 
// structure is used to pass multiple arguments.
//****************************************************************************
#include <stdio.h>
#include <hpx/hpx.h>

#define NUM_THREADS                5
static  hpx_action_t _main       = 0;
static  hpx_action_t _printHello = 0;

char *messages[NUM_THREADS];

struct thread_data {
  int thread_id;
  char *message;
};

struct thread_data thread_data_array[NUM_THREADS];

static int _printHello_action(size_t size, void *threadarg) {
  int tid;
  char *hello_msg;
  struct thread_data *my_data;

  my_data = (struct thread_data *)threadarg;
  tid = my_data->thread_id;
  hello_msg = my_data->message;

  printf("Thread #%d: size of args = %lu, Message = %s\n", tid, size, hello_msg);
  hpx_thread_continue(0, NULL);
}

//****************************************************************************
// @Action which spawns the threads
//****************************************************************************
static int _main_action(void *args) {
  messages[0] = "English: Hello World!";
  messages[1] = "French:  Bonjour, le monde!";
  messages[2] = "German:  Guten Tag, Welt!"; 
  messages[3] = "Japan:   Sekai e konnichiwa!";
  messages[4] = "Latin:   Orbis, te saluto!";

  hpx_addr_t and = hpx_lco_and_new(NUM_THREADS);
  for (int i = 0; i < NUM_THREADS; i++) {
    thread_data_array[i].thread_id = i;
    thread_data_array[i].message = messages[i];
    hpx_call(HPX_HERE, _printHello, and, &thread_data_array[i],
             sizeof(thread_data_array));
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_SIZE_T, HPX_POINTER);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _printHello, _printHello_action, HPX_SIZE_T, HPX_POINTER);

  return hpx_run(&_main, NULL, 0);
}
