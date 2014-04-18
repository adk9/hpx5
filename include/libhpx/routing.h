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

typedef struct of_switch switch_t;
typedef struct routing routing_t;

#define HPX_SWADDR_WILDCARD 0x0

#define block_id_macaddr(b) ((uint64_t)((uint64_t)0x01005e << 24)|b);
#define block_id_ipv4mc(b) ((uint64_t)((uint64_t)0xe << 28)|b);

HPX_INTERNAL routing_t *routing_new(void);
HPX_INTERNAL void routing_delete(routing_t*) HPX_NON_NULL(1);
HPX_INTERNAL int routing_add_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) HPX_NON_NULL(1);
HPX_INTERNAL int routing_delete_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) HPX_NON_NULL(1);
HPX_INTERNAL int routing_update_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) HPX_NON_NULL(1);
HPX_INTERNAL int routing_my_port(const routing_t *r) HPX_NON_NULL(1);
HPX_INTERNAL int routing_register_addr(const routing_t *r, uint64_t addr) HPX_NON_NULL(1);
HPX_INTERNAL int routing_unregister_addr(const routing_t *r, uint64_t addr) HPX_NON_NULL(1);

#endif // LIBHPX_NETWORK_ROUTING_H
