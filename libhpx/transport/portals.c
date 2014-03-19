/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Portals Network Interface 
  portals.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <stdlib.h>
#include <portals4.h>
#include "network."
#include "hpx/error.h"

static int init(void);
static int finalize(void);
static int probe(int source, int* flag, network_status_t* status);
static int send(int dest, void *buffer, size_t len, network_request_t* req);
static int recv(int src, void *buffer, size_t len, network_request_t* req);
static int put(int dest, void *buffer, size_t len, network_request_t* req);
static int get(int src, void *buffer, size_t len, network_request_t* req);
static int test(network_request_t *request, int *flag, network_status_t *status);
static int pin(void* buffer, size_t len);
static int unpin(void* buffer, size_t len);
static int phys_addr(hpx_locality_t *l);
static void progress(void *data);
static size_t get_network_bytes(size_t n);
static void barrier(void);

/* Portals communication operations */
network_ops_t portals_ops = {
  .init              = init,
  .finalize          = finalize,
  .progress          = progress,
  .probe             = probe,
  .send              = send,
  .recv              = recv,
  .test              = test,
  .put               = put,
  .get               = get,
  .phys_addr         = phys_addr,
  .get_network_bytes = get_network_bytes,
  .barrier           = barrier
};

int
init(void)
{
}

int
send_parcel(hpx_locality_t *, hpx_parcel_t *)
{
}

int
send(int peer, void *payload, size_t len)
{
}

int
put(int peer, void *dst, void *src, size_t len)
{
}

int
get(void *dst, int peer, void *src, size_t len)
{
}

void
progress(void *data)
{
}

void
finalize(void)
{
}

int
probe(int source, int* flag, network_status_t *status)
{
}

int
test(network_request_t *request, int *flag, network_status_t *status)
{
}

int
phys_addr(hpx_locality_t *id)
{
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
