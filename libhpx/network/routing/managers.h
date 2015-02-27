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
#ifndef LIBHPX_NETWORK_ROUTING_MANAGERS_H
#define LIBHPX_NETWORK_ROUTING_MANAGERS_H

#include <inttypes.h>

struct routing {
  void (*delete)(routing_t*);
  int (*add_flow)(const routing_t*, uint64_t, uint64_t, uint16_t);
  int (*delete_flow)(const routing_t*, uint64_t, uint64_t, uint16_t);
  int (*update_flow)(const routing_t*, uint64_t, uint64_t, uint16_t);
  int (*my_port)(const routing_t*);
  int (*register_addr)(const routing_t*, uint64_t);
  int (*unregister_addr)(const routing_t*, uint64_t);
};

// This structure represents an openflow switch, its features and it status.
struct of_switch {
  void     *dpid;
  uint32_t  nbuffers;
  uint8_t   ntables;
  uint32_t  capabilities;
  uint32_t  actions;
  void     *ports;
  bool      active;
  switch_t *next;
};

HPX_INTERNAL routing_t *routing_new_trema(void);
HPX_INTERNAL routing_t *routing_new_floodlight(void);
HPX_INTERNAL routing_t *routing_new_dummy(void);

#endif // LIBHPX_NETWORK_ROUTING_MANAGERS_H
