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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// OpenFlow Controller API.
///
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>

#include "hpx/hpx.h"
#include "libsync/locks.h"
#include "libsync/hashtables.h"
#include "libhpx/debug.h"
#include "libhpx/scheduler.h"
#include "libhpx/routing.h"
#include "contrib/uthash/src/utlist.h"

#include "openflow/openflow.h"

typedef struct openflow openflow_t;
struct openflow {
  routing_class_t  class;
  switch_t        *myswitch;
  switch_t        *switches;            // List of available switches.
};


// Query the switch information
static void _query_switch(uint64_t dpid, void *args) {
}

static void _get_switches(const list_element *switches, void *args) {
}

static bool _cmp_dpid(switch_t *s, uint64_t dpid) {
  return (*(uint64_t*)s->dpid == dpid);
}

// Add switch information
static void _add_switch(uint64_t dpid, uint32_t tid, uint32_t nbuf,
                        uint8_t ntab, uint32_t cap, uint32_t actions,
                        const list_element *phy_ports, void *args) {
}

static void _disable_switch(uint64_t dpid, void *args) {
}

static void _packet_handler(uint64_t dpid, packet_in message) {
}

static void _delete(routing_class_t *routing) {
  const openflow_t *openflow = (const openflow_t*)routing;
}

static int _send_of_msg(const routing_class_t *r, const uint16_t cmd, struct ofp_match *match, uint16_t port) {
  const openflow_t *openflow = (const openflow_t*)r;
}

static int _add_flow(const routing_class_t *r, uint64_t src, uint64_t dst, uint16_t port) {
}

static int _delete_flow(const routing_class_t *r, uint64_t src, uint64_t dst, uint16_t port) {
}

static int _update_flow(const routing_class_t *r, uint64_t src, uint64_t dst, uint16_t port) {
}

static int _my_port(const routing_class_t *r) {
}

static int _register_addr(const routing_class_t *r, uint64_t addr) {
}

static int _unregister_addr(const routing_class_t *r, uint64_t addr) {
}

routing_class_t *routing_new_openflow(void) {
  openflow_t *openflow = malloc(sizeof(*openflow));
  openflow->class.type            = HPX_ROUTING_OPENFLOW;
  openflow->class.delete          = _delete;
  openflow->class.add_flow        = _add_flow;
  openflow->class.delete_flow     = _delete_flow;
  openflow->class.update_flow     = _update_flow;
  openflow->class.my_port         = _my_port;
  openflow->class.register_addr   = _register_addr;
  openflow->class.unregister_addr = _unregister_addr;

  openflow->myswitch = NULL;
  openflow->switches = NULL;
  
  return &openflow->class;
}
