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

#include "managers.h"
#include "trema.h"


#if HAVE_PHOTON
  #include "bravo_ids.h"
#endif

typedef struct trema trema_t;
struct trema {
  routing_t vtable;
  switch_t *myswitch;
  switch_t *switches;   // List of available switches.
  pthread_t cthread;    // The OF controller thread.
};


// Query the switch information
static void _query_switch(uint64_t dpid, void *args) {
  uint32_t tid = get_transaction_id();
  buffer *msg = create_features_request(tid);
  send_openflow_message(dpid, msg);
  free_buffer(msg);
}

static void _get_switches(const list_element *switches, void *args) {
  const list_element *e;
  uint64_t dpid;
  for (e = switches; e != NULL; e = e->next) {
    dpid = (uint64_t)e->data;
    _query_switch(dpid, 0);
  }
}

static bool _cmp_dpid(switch_t *s, uint64_t dpid) {
  return (*(uint64_t*)s->dpid == dpid);
}

// Add switch information
static void _add_switch(uint64_t dpid, uint32_t tid, uint32_t nbuf,
                        uint8_t ntab, uint32_t cap, uint32_t actions,
                        const list_element *phy_ports, void *args) {
  const trema_t *trema = (const trema_t*)args;
  // add switch information to a switch table
  switch_t *s = NULL;
  LL_SEARCH(trema->switches, s, dpid, _cmp_dpid);
  if (!s) {
    s = malloc(sizeof(*s));
    LL_PREPEND(((trema_t*)trema)->switches, s);
    log_net("trema: adding new switch 0x%016lx.\n", dpid);
  }

  s->dpid = malloc(sizeof(dpid));
  memcpy(s->dpid, &dpid, sizeof(dpid));
  s->nbuffers     = nbuf;
  s->ntables      = ntab;
  s->capabilities = cap;
  s->actions      = actions;
  s->active       = true;

  int nports = list_length_of(phy_ports);
  s->ports = malloc(sizeof(struct ofp_phy_port) * nports);

  list_element *p;
  int i = 0;
  for (p = (list_element*)phy_ports; p != NULL; p = p->next) {
    memcpy(s->ports + i, p->data, sizeof(struct ofp_phy_port));
    i += sizeof(struct ofp_phy_port);
  }

  // FIXME:
  ((trema_t*)trema)->myswitch = ((trema_t*)trema)->switches;
  
  // The default rules for the switch would be added here.  
}

static void _disable_switch(uint64_t dpid, void *args) {
  const trema_t *trema = (const trema_t*)args;
  switch_t *s = NULL;
  LL_SEARCH(trema->switches, s, dpid, _cmp_dpid);
  if (s)
    s->active = false;
}

static void _packet_handler(uint64_t dpid, packet_in message) {
  info( "received a packet_in" );
  info( "datapath_id: %#" PRIx64, dpid );
  info( "transaction_id: %#x", message.transaction_id );
  info( "buffer_id: %#x", message.buffer_id );
  info( "total_len: %u", message.total_len );
  info( "in_port: %u", message.in_port );
  info( "reason: %#x", message.reason );
  info( "data:" );
  dump_buffer( message.data, info );
}

static void *_start(void *args) {
  const trema_t *trema = (const trema_t*)args;
  int argc = 0;
  char **argv;
  // initialize trema
  init_trema(&argc, &argv);

  // register switch management callbacks
  //set_list_switches_reply_handler(_get_switches);
  _set_switch_ready_handler(false, (void *)_query_switch, NULL);
  //set_features_reply_handler(_add_switch, (void *)trema);
  //set_switch_disconnected_handler(_disable_switch, (void *)trema);
  _set_packet_in_handler(true, (void *)_packet_handler, NULL);

  // send the list switches request
  //send_list_switches_request(NULL);

  // this blocks the thread permanently. that is why it must be called
  // from within a different thread.
  start_trema();
  pthread_exit(NULL);
}

static void _delete(routing_t *routing) {
  const trema_t *trema = (const trema_t*)routing;
  switch_t *s = NULL, *tmp = NULL;
  // delete all switches
  LL_FOREACH_SAFE(((trema_t*)trema)->switches, s, tmp) {
    LL_DELETE(((trema_t*)trema)->switches, s);
    free(s);
  }

  stop_trema();

  // wait for the OF thread to shutdown
  int e = pthread_join(trema->cthread, NULL);
  if (e)
    dbg_error("trema: could not join the OF controller thread.\n");
}

static int _send_of_msg(const routing_t *r, const uint16_t cmd, struct ofp_match *match, uint16_t port) {
  const trema_t *trema = (const trema_t*)r;
  switch_t *s = trema->myswitch;
  assert(s);

  openflow_actions *actions = create_actions();
  append_action_output(actions, port, UINT16_MAX);

  uint32_t tid          = get_transaction_id();
  uint32_t cookie       = get_cookie();
  uint16_t idle_timeout = 0;
  uint16_t hard_timeout = 0;
  uint16_t priority     = UINT16_MAX;
  uint32_t buffer_id    = UINT32_MAX;

  // and then create the flow rule
  buffer *flow = create_flow_mod(tid, *match, cookie, cmd, idle_timeout,
                                 hard_timeout, priority, buffer_id, OFPP_NONE,
                                 0, actions);
  send_openflow_message(*(uint64_t*)s->dpid, flow);
  free_buffer(flow);
  delete_actions(actions);
  return HPX_SUCCESS;  
}

static int _add_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  // create the match request
  struct ofp_match match;
  log_net("trema: adding flow, output=%d.\n", port);
  memset(&match, 0, sizeof(match));
  match.wildcards = (OFPFW_ALL & ~(OFPFW_DL_SRC|OFPFW_DL_DST));
  if (src != HPX_SWADDR_WILDCARD)
    memcpy(match.dl_src, &src, OFP_ETH_ALEN);
  if (dst != HPX_SWADDR_WILDCARD)
    memcpy(match.dl_dst, &dst, OFP_ETH_ALEN);
  return _send_of_msg(r, OFPFC_ADD, &match, port);
}

static int _delete_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
    // create the match request
  struct ofp_match match;
  memset(&match, 0, sizeof(match));
  match.wildcards = (OFPFW_ALL & ~(OFPFW_DL_SRC|OFPFW_DL_DST));
  if (src != HPX_SWADDR_WILDCARD)
    memcpy(match.dl_src, &src, OFP_ETH_ALEN);
  if (dst != HPX_SWADDR_WILDCARD)
    memcpy(match.dl_dst, &dst, OFP_ETH_ALEN);
  return _send_of_msg(r, OFPFC_DELETE_STRICT, &match, port);
}

static int _update_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  // create the match request
  struct ofp_match match;
  memset(&match, 0, sizeof(match));
  match.wildcards = (OFPFW_ALL & ~(OFPFW_DL_SRC|OFPFW_DL_DST));
  if (src != HPX_SWADDR_WILDCARD)
    memcpy(match.dl_src, &src, OFP_ETH_ALEN);
  if (dst != HPX_SWADDR_WILDCARD)
    memcpy(match.dl_dst, &dst, OFP_ETH_ALEN);
  return _send_of_msg(r, OFPFC_MODIFY_STRICT, &match, port);
}

static int _my_port(const routing_t *r) {
  photon_addr addr;
  photon_get_dev_addr(AF_INET, &addr);
  bravo_node *b = find_bravo_node(&addr);
  assert(b);
  return b->of_port;
}

static int _register_addr(const routing_t *r, uint64_t addr) {
  photon_addr daddr = {.blkaddr.blk3 = addr};
  return photon_register_addr(&daddr, AF_INET6);
}

static int _unregister_addr(const routing_t *r, uint64_t addr) {
  photon_addr daddr = {.blkaddr.blk3 = addr};
  return photon_unregister_addr(&daddr, AF_INET6);
}

routing_t *routing_new_trema(void) {
  trema_t *trema = malloc(sizeof(*trema));
  trema->vtable.delete          = _delete;
  trema->vtable.add_flow        = _add_flow;
  trema->vtable.delete_flow     = _delete_flow;
  trema->vtable.update_flow     = _update_flow;
  trema->vtable.my_port         = _my_port;
  trema->vtable.register_addr   = _register_addr;
  trema->vtable.unregister_addr = _unregister_addr;

  trema->myswitch = NULL;
  trema->switches = NULL;

  int e = pthread_create(&trema->cthread, NULL, _start, trema);
  if (e) {
    dbg_error("trema: could not start the OF controller thread.\n");
    return NULL;
  }
  
  return &trema->vtable;
}
