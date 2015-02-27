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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <assert.h>

#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"



rkey_t *new_rkey(transport_t *t, char *heap_base) {
  //int bytes = transport_rkey_size(t);
  rkey_t *r = malloc(sizeof(*r));
  if (!r) {
    dbg_error("transport: could not allocate registration key.\n");
    return NULL;
  }
  r->base = heap_base;
  return r;
}


rkey_t *exchange_rkey_table(transport_t *t, rkey_t *my_rkey) {
  // allocate the rkey table
  int bytes = sizeof(*my_rkey);
  rkey_t *rkey_table = malloc(bytes * here->ranks);
  if (!rkey_table) {
    dbg_error("transport: could not allocate registration key table.\n");
    return NULL;
  }

  // do the key exchange
  int e = boot_allgather(here->boot, (void*)my_rkey, rkey_table, bytes);
  if (e) {
    dbg_error("transport: error exchanging registration keys.\n");
    return NULL;
  }

  // wait for the exchange to happen on all localities
  boot_barrier(here->boot);
  return rkey_table;
}
