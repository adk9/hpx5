/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Network Simulation Interface 
  sim.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#ifdef __linux__
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "bootstrap/bootstrap.h"
#include "network.h"
#include "hpx/action.h"
#include "hpx/init.h"
#include "hpx/parcel.h"

#include "network_sim.h"

int _argc;
char **_argv;
char *_argv_buffer;

/* SIM network operations */
network_ops_t sim_ops = {
    .init             = sim_init,
    .finalize         = sim_finalize,
    .progress         = sim_progress,
    .probe            = sim_probe,
    .send             = sim_send,
    .recv             = sim_recv,
    .sendrecv_test    = sim_test,
    .put              = sim_put,
    .get              = sim_get,
    .putget_test      = sim_test,
    .pin              = sim_pin,
    .unpin            = sim_unpin,
    .phys_addr        = sim_phys_addr,
};

int _rank_sim;
int _size_sim;

int sim_init(void) {
  int ret;

  /* cache size and rank */
  _rank_sim = bootmgr->get_rank();
  _size_sim = bootmgr->size();

  /* The initialization phase in the runtime should do the argv
   * handling and parsing of the flags passed in. These could be set
   * in a configuration struct at start-up. Here, we need to check if
   * the simulation flag was specified or not. */

  return ret;
}

/* status may NOT be NULL */
int sim_probe(int source, int* flag, network_status_t* status) {
  int ret;
  return ret;
}

/* Send data. */
int sim_send(int dest, void *data, size_t len, network_request_t *request) {
  int ret;
  int temp;
  return retval;  
}

/* this is non-blocking recv - user must test/wait on the request */
int sim_recv(int source, void* buffer, size_t len, network_request_t *request) {
}

/* status may be NULL */
int sim_test(network_request_t *request, int *flag, network_status_t *status) {
}

int sim_put(int dest, void *buffer, size_t len, network_request_t *request) {
  return HPX_ERROR;
}

int sim_get(int src, void *buffer, size_t len, network_request_t *request) {
  return HPX_ERROR;
}

/* Return the physical network ID of the current process */
int sim_phys_addr(hpx_locality_t *l) {
}

void sim_progress(void *data) {
}

int sim_finalize(void) {
}

int sim_pin(void* buffer, size_t len) {
  return 0;
}

int sim_unpin(void* buffer, size_t len) {
  return 0;
}
