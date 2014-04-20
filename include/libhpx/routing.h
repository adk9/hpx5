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
#ifndef LIBHPX_NETWORK_ROUTING_H
#define LIBHPX_NETWORK_ROUTING_H

#include <stdint.h>
#include "hpx/attributes.h"
#include "hpx/hpx.h"

#define HPX_SWADDR_WILDCARD 0x0

#define block_id_macaddr(b) ((uint64_t)((uint64_t)0x01005e << 24)|b);
#define block_id_ipv4mc(b) ((uint64_t)((uint64_t)0xe << 28)|b);

typedef struct of_switch switch_t;
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

typedef struct routing_class routing_class_t;
struct routing_class {
  hpx_routing_t type;
  void (*delete)(routing_class_t*);
  int (*add_flow)(const routing_class_t*, uint64_t, uint64_t, uint16_t);
  int (*delete_flow)(const routing_class_t*, uint64_t, uint64_t, uint16_t);
  int (*update_flow)(const routing_class_t*, uint64_t, uint64_t, uint16_t);
  int (*my_port)(const routing_class_t*);
  int (*register_addr)(const routing_class_t*, uint64_t);
  int (*unregister_addr)(const routing_class_t*, uint64_t);
};


HPX_INTERNAL routing_class_t *routing_new_trema(void);
HPX_INTERNAL routing_class_t *routing_new_floodlight(void);
HPX_INTERNAL routing_class_t *routing_new_dummy(void);
HPX_INTERNAL routing_class_t *routing_new(hpx_routing_t type);


static inline void routing_delete(routing_class_t *routing) {
  routing->delete(routing);
}

static inline int routing_add_flow(const routing_class_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  return r->add_flow(r, src, dst, port);
}

static inline int routing_delete_flow(const routing_class_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  return r->delete_flow(r, src, dst, port);
}

static inline int routing_update_flow(const routing_class_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  return r->update_flow(r, src, dst, port);
}

static inline int routing_my_port(const routing_class_t *r) {
  return r->my_port(r);
}

static inline int routing_register_addr(const routing_class_t *r, uint64_t addr) {
  return r->register_addr(r, addr);
}

static inline int routing_unregister_addr(const routing_class_t *r, uint64_t addr) {
  return r->unregister_addr(r, addr);
}


#endif // LIBHPX_NETWORK_ROUTING_H
