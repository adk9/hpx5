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
#include "network.h"
#include "hpx/error.h"
#include "hpx/globals.h"                        /* bootmgr */
#include "bootstrap.h"                          /* struct hpx_bootstrap */

static int  init(void);
static int  finalize(void);
static void progress(void *data);
static int  probe(int source, int* flag, network_status_t* status);
static int  send(int dest, void *data, size_t len, network_request_t *request);
static int  recv(int src, void *buffer, size_t len, network_request_t *request);
static int  test(network_request_t *request, int *flag, network_status_t *status);
static int  put(int dest, void *buffer, size_t len, network_request_t *request);
static int  get(int src, void *buffer, size_t len, network_request_t *request);
static int  pin(void* buffer, size_t len);
static int  unpin(void* buffer, size_t len);
static int  phys_addr(hpx_locality_t *id);
static size_t get_network_bytes(size_t n);
static void barrier(void);

/* SIM network operations */
network_ops_t sim_ops = {
  .init              = init,
  .finalize          = finalize,
  .progress          = progress,
  .probe             = probe,
  .send              = send,
  .recv              = recv,
  .sendrecv_test     = test,
  .put               = put,
  .get               = get,
  .putget_test       = test,
  .pin               = pin,
  .unpin             = unpin,
  .phys_addr         = phys_addr,
  .get_network_bytes = get_network_bytes,
  .barrier           = barrier
};

static int _rank_sim;
static int _size_sim;

int
init(void)
{
  int ret = HPX_SUCCESS;

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
int
probe(int source, int* flag, network_status_t* status)
{
  return HPX_ERROR;
}

/* Send data. */
int
send(int dest, void *data, size_t len, network_request_t *request)
{
  return HPX_ERROR;
}

/* this is non-blocking recv - user must test/wait on the request */
int
recv(int source, void* buffer, size_t len, network_request_t *request)
{
  return HPX_ERROR;
}

/* status may be NULL */
int
test(network_request_t *request, int *flag, network_status_t *status)
{
  return HPX_ERROR;
}

int
put(int dest, void *buffer, size_t len, network_request_t *request)
{
  return HPX_ERROR;
}

int
get(int src, void *buffer, size_t len, network_request_t *request)
{
  return HPX_ERROR;
}

/* Return the physical network ID of the current process */
int
phys_addr(hpx_locality_t *l)
{
  return HPX_ERROR;
}

void
progress(void *data)
{
}

int
finalize(void)
{
  return HPX_ERROR;
}

int
pin(void* buffer, size_t len)
{
  return 0;
}

int
unpin(void* buffer, size_t len)
{
  return 0;
}

size_t
get_network_bytes(size_t n)
{
  return n;
}

void
barrier(void)
{
}
