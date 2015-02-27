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
#include <config.h>
#endif

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <portals4.h>

#include "libhpx/btt.h"
#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"
#include "hpx/hpx.h"

static hpx_action_t _send_progress_action;
static hpx_action_t _recv_progress_action;

/// Portals resource limits
static const int PORTALS_RES_LIMIT_MAX = INT_MAX;
typedef struct _portals_lim _portals_lim_t;
struct _portals_lim {
  ptl_ni_limits_t rlim_cur;  // Soft limit (currently available)
  ptl_ni_limits_t rlim_max;  // Hard limit (requested)
};

static int   portals_default_srlimit = 32;

static const uint64_t PORTALS_EVENTQ_SIZE = UINT_MAX;
static const uint64_t PORTALS_BUFFER_SIZE = (1UL << 20);

/// Portals data buffer
static const int PORTALS_NUM_BUF_MAX   = 16;
typedef struct _portals_buf _portals_buf_t;
struct _portals_buf {
  ptl_handle_me_t handle;
  void           *data;
};


/// the Portals transport.
typedef struct {
  transport_t class;
  ptl_process_t     id;                 // my (physical) ID
  _portals_lim_t    limits;             // portals transport resource limits
  ptl_handle_ni_t   interface;          // handle to the non-matching interface

  ptl_handle_eq_t   sendq;              // the send event queue
  ptl_handle_eq_t   recvq;              // the recv event queue
  ptl_pt_index_t    pte;                // table entry for messages
  ptl_handle_md_t   bufdesc;            // buffer descriptor
  _portals_buf_t   *buffer;             // parcel buffer
} portals_t;


/// ----------------------------------------------------------------------------
/// Get the ID for the Portals transport.
/// ----------------------------------------------------------------------------
static const char *_id(void) {
  return "Portals 4";
}

/// ----------------------------------------------------------------------------
/// Get the physical ID for the Portals transport.
///
/// Presently, Portals relies on the IPoIB transport to get a valid IP
/// address to use as the physical identifier of the locality.
/// ----------------------------------------------------------------------------
static int _get_physical_id(portals_t *portals, ptl_process_t *id) {
  assert(portals != NULL);
  if (PtlGetPhysId(portals->interface, id) != PTL_OK)
    return dbg_error("could not get the physical id.\n");
  return HPX_SUCCESS;
}

/// ----------------------------------------------------------------------------
/// Initialize the Portals transport.
///
/// At the moment, we only initialize and use the "matching"
/// interface. Maximum resource limits are requested during
/// initialization.
/// ----------------------------------------------------------------------------
static int _portals_init(portals_t *portals) {
  assert(portals != NULL);

  int e = PtlInit();
  if (e != PTL_OK)
    return dbg_error("failed to initialize Portals.\n");

  ptl_ni_limits_t *nl = &(portals->limits.rlim_max);

  nl->max_entries            = PORTALS_RES_LIMIT_MAX;
  nl->max_unexpected_headers = PORTALS_RES_LIMIT_MAX;
  nl->max_mds                = PORTALS_RES_LIMIT_MAX;
  nl->max_eqs                = PORTALS_RES_LIMIT_MAX;
  nl->max_cts                = PORTALS_RES_LIMIT_MAX;
  nl->max_pt_index           = PORTALS_RES_LIMIT_MAX;
  nl->max_iovecs             = PORTALS_RES_LIMIT_MAX;
  nl->max_list_size          = PORTALS_RES_LIMIT_MAX;
  nl->max_triggered_ops      = PORTALS_RES_LIMIT_MAX;
  nl->max_msg_size           = LONG_MAX;
  nl->max_atomic_size        = PORTALS_RES_LIMIT_MAX;
  nl->max_fetch_atomic_size  = PORTALS_RES_LIMIT_MAX;
  nl->max_waw_ordered_size   = PORTALS_RES_LIMIT_MAX;
  nl->max_war_ordered_size   = PORTALS_RES_LIMIT_MAX;
  nl->max_volatile_size      = PORTALS_RES_LIMIT_MAX;
  nl->features               = 0;

  e = PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING|PTL_NI_LOGICAL, PTL_PID_ANY,
                nl, &(portals->limits.rlim_cur), &portals->interface);
  if (e != PTL_OK)
    return dbg_error("failed to initialize Portals.\n");

  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Set the portals locality map (Rank X PhysID) appropriately.
/// ----------------------------------------------------------------------------
static int _set_map(portals_t *ptl) {
  ptl_process_t *nidpid_map = malloc(sizeof(*nidpid_map) * here->ranks);

#ifdef HAVE_PMI_CRAY_EXT
  if ((PMI_Portals_get_nidpid_map(&nidpid_map)) != PMI_SUCCESS) {
    free(nidpid_map);
    return dbg_error("failed to get nidpid map from PMI.\n");
  }
#else
  _get_physical_id(ptl, &ptl->id);
  boot_allgather(here->boot, (void*)&ptl->id, nidpid_map,
                 sizeof(*nidpid_map));
#endif

  int e = PtlSetMap(ptl->interface, here->ranks, nidpid_map);
  if (e != PTL_OK) {
    free(nidpid_map);
    dbg_error("failed to set portals nidpid map.\n");
  }

  free(nidpid_map);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// This sets up Portals for sending and receiving messages. We create
/// event queues for send and receive events, set up portal table
/// entries, bind memory descriptors and append match-list entries to
/// the priority list.
/// ----------------------------------------------------------------------------
static int _setup_portals(portals_t *ptl) {
  // create an event queue for send events
  int e = PtlEQAlloc(ptl->interface, PORTALS_EVENTQ_SIZE, &ptl->sendq);
  if (e != PTL_OK)
    return dbg_error("failed to allocate portals send event queue.\n");

  // create an event queue for receive events
  e = PtlEQAlloc(ptl->interface, PORTALS_EVENTQ_SIZE, &ptl->recvq);
  if (e != PTL_OK)
    return dbg_error("failed to allocate portals receive event queue.\n");

  // create a portal table entry for receiving data
  e = PtlPTAlloc(ptl->interface, 0, ptl->recvq, PTL_PT_ANY, &ptl->pte);
  if (e != PTL_OK)
    return dbg_error("failed to allocate portals table entry.\n");

  // allocate a memory descriptor. (we assume that the underlying
  // Portals 4 implementation supports binding a descriptor that spans
  // the entire virtual address-space.)
  ptl_md_t bd = {
    .start     = 0,
    .length    = PTL_SIZE_MAX,
    .options   = PTL_MD_UNORDERED | PTL_MD_EVENT_SEND_DISABLE,
    .eq_handle = ptl->sendq,
    .ct_handle = PTL_CT_NONE,
  };
  e = PtlMDBind(ptl->interface, &bd, &ptl->bufdesc);
  if (e != PTL_OK)
    return dbg_error("failed to allocate memory buffer descriptor.\n");

  // append match-list entries for parcel buffers
  ptl->buffer = malloc(sizeof(_portals_buf_t) * PORTALS_NUM_BUF_MAX);
  for (int i = 0; i < PORTALS_NUM_BUF_MAX; i++) {
    _portals_buf_t *b = &ptl->buffer[i];
    b->data = malloc(PORTALS_BUFFER_SIZE);

    ptl_me_t me = {
      .start         = b->data,
      .length        = PORTALS_BUFFER_SIZE,
      .ct_handle     = PTL_CT_NONE,
      .uid           = PTL_UID_ANY,
      .options       = PTL_ME_OP_PUT | PTL_ME_MANAGE_LOCAL
                     | PTL_ME_MAY_ALIGN | PTL_ME_EVENT_LINK_DISABLE,
      .match_id.rank = PTL_RANK_ANY,
      .match_bits    = 0,
      .ignore_bits   = ~(0ULL),
      .min_free      = 1024, // need space for a few parcels at least?
    };
    e = PtlMEAppend(ptl->interface, ptl->pte, &me, PTL_PRIORITY_LIST,
                    b, &b->handle);
    if (e != PTL_OK)
      return dbg_error("failed to append match list entry.\n");
  }
  return HPX_SUCCESS;
}

/// ----------------------------------------------------------------------------
/// A global synchronizing barrier.
/// ----------------------------------------------------------------------------
static void _barrier(void) {
  log_trans("barrier unsupported.");
}

/// ----------------------------------------------------------------------------
/// Return the size of a Portals request.
/// ----------------------------------------------------------------------------
static int _request_size(void) {
  return 0;
}


/// ----------------------------------------------------------------------------
/// Return the size of the Portals registration key.
/// ----------------------------------------------------------------------------
static int _rkey_size(void) {
  return 0;
}


static int _adjust_size(int size) {
  return size;
}

/// ----------------------------------------------------------------------------
/// Cancel an active transport request.
/// ----------------------------------------------------------------------------
static int _request_cancel(void *request) {
  return 0;
}

/// ----------------------------------------------------------------------------
/// Pinning not necessary.
/// ----------------------------------------------------------------------------
static int _pin(transport_class_t *transport, const void* buffer, size_t len) {
  return LIBHPX_OK;
}

/// ----------------------------------------------------------------------------
/// Unpinning not necessary.
/// ----------------------------------------------------------------------------
static void _unpin(transport_t *transport, const void* buffer, size_t len) {
}

/// ----------------------------------------------------------------------------
/// Put data via Portals.
/// ----------------------------------------------------------------------------
static int _put(transport_t *t, int dest, const void *data, size_t n,
                void *rbuffer, size_t rn, void *rid, void *r)
{
  log_trans("put unsupported.\n");
  return HPX_SUCCESS;
}

/// ----------------------------------------------------------------------------
/// Get data via Portals.
/// ----------------------------------------------------------------------------
static int _get(transport_t *t, int dest, void *buffer, size_t n,
                const void *rdata, size_t rn, void *rid, void *r)
{
  log_trans("get unsupported.\n");
  return HPX_SUCCESS;
}

/// ----------------------------------------------------------------------------
/// Send data via Portals.
/// ----------------------------------------------------------------------------
static int _send(transport_t *t, int dest, const void *data, size_t n, void *r)
{
  log_trans("send unsupported.\n");
  return HPX_SUCCESS;
}

/// ----------------------------------------------------------------------------
/// Test for request completion.
/// ----------------------------------------------------------------------------
static int _test(transport_class_t *t, void *request, int *success) {
  log_trans("test unsupported.\n");
  return HPX_SUCCESS;
}

/// ----------------------------------------------------------------------------
/// Probe the Portals transport to see if anything has been received.
/// ----------------------------------------------------------------------------
static size_t _probe(transport_class_t *t, int *source) {
  log_trans("probe unsupported.\n");
  return 0;
}

/// ----------------------------------------------------------------------------
/// Receive a buffer.
/// ----------------------------------------------------------------------------
static int _recv(transport_class_t *t, int src, void* buffer, size_t n, void *r) {
  log_trans("recv unsupported.\n");
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Shut down Portals, and delete the transport.
/// ----------------------------------------------------------------------------
static void _delete(transport_t *transport) {
  portals_t *ptl = (portals_t*)transport;

  for (int i = 0; i < PORTALS_NUM_BUF_MAX; ++i)
    PtlMEUnlink(ptl->buffer[i].handle);
  free(ptl->buffer);

  PtlMDRelease(ptl->bufdesc);
  PtlPTFree(ptl->interface, ptl->pte);

  PtlEQFree(ptl->sendq);
  PtlEQFree(ptl->recvq);

  PtlNIFini(ptl->interface);
  PtlFini();
}

static bool _try_start_send(portals_t *ptl) {
  // try to deque a packet from the network's Tx port.
  hpx_parcel_t *p = network_tx_dequeue(here->network);
  if (!p)
    return false;

  uint32_t dest = btt_owner(here->btt, p->target);
  ptl_process_t peer = { .rank = dest };

  int size = sizeof(*p) + p->size;
  //int e = PtlPut(ptl->bufdesc, (ptl_size_t)p, size, PTL_ACK_REQ, peer,
  //               ptl->pte, 0, 0, p, 0);
  int e = PtlPut(ptl->bufdesc, (ptl_size_t)p, size, PTL_ACK_REQ, peer,
                 ptl->pte, 0, 0, p, 0);
  if (e != PTL_OK) {
    hpx_parcel_release(p);
    dbg_error("Portals could not send %d bytes to %i.\n", size, dest);
    return false;
  }

  return true;
}

static int _handle_send_event(ptl_event_t *pe) {
  hpx_parcel_t *p = (hpx_parcel_t*)pe->user_ptr;
  switch (pe->type) {
    default:
      log_trans("unknown send queue event (%d).\n", pe->type);
      return PTL_FAIL;
    case PTL_EVENT_ACK:
      if (pe->ni_fail_type == PTL_NI_OK)
        hpx_parcel_release(p);
      else {
        dbg_error("Portals failed to ack send of %u bytes to %i.\n",
                  p->size, btt_owner(here->btt, p->target));
        // perhaps we should try to retransmit?
        // network_tx_enqueue(here->network, p);
        return pe->ni_fail_type;
      }
      break;
    case PTL_EVENT_SEND:
      if (pe->ni_fail_type != PTL_NI_OK) {
        dbg_error("Portals failed to send %u bytes to %i.\n",
                  p->size, btt_owner(here->btt, p->target));
        // perhaps we should try to retransmit?
        // network_tx_enqueue(here->network, p);
        return pe->ni_fail_type;
      }
      break;
  }
  return PTL_OK;
}

static int _send_progress(transport_t **t) {
  portals_t *ptl = (portals_t*)*t;

  bool send = _try_start_send(ptl);
  if (send)
    log_trans("started a send.\n");

  ptl_event_t pe;
  int e = PtlEQGet(ptl->sendq, &pe);
  if (e == PTL_OK)
    _handle_send_event(&pe);

  return HPX_SUCCESS;
}

static void _send_flush(transport_t *t) {
  portals_t *ptl = (portals_t*)t;

  bool send = true;
  while (send)
    send = _try_start_send(ptl);

  int e;
  ptl_event_t pe;
  do {
    e = PtlEQGet(ptl->sendq, &pe);
    if (e == PTL_OK)
      _handle_send_event(&pe);
  } while (e != PTL_EQ_EMPTY);
}

static int _recv_progress(transport_t **t) {
  portals_t *ptl = (portals_t*)*t;
  ptl_event_t pe;

  int e = PtlEQGet(ptl->recvq, &pe);
  if (e == PTL_OK) {
    if (pe.type == PTL_EVENT_PUT) {
      if (pe.ni_fail_type != PTL_NI_OK) {
        dbg_error("Portals failed to recv %lu bytes from %i.\n",
                  pe.mlength, pe.initiator.rank);
      }
      else {
        assert(pe.rlength == pe.mlength);

        // allocate a parcel to provide the buffer to receive into
        hpx_parcel_t *p = hpx_parcel_acquire(NULL, pe.mlength - sizeof(hpx_parcel_t));
        if (!p)
          dbg_error("could not acquire a parcel of size %lu during receive.\n", pe.mlength);

        // TODO: get rid of this extra copy.
        memcpy(p, pe.start, pe.mlength);
        network_rx_enqueue(here->network, p);
      }
    }
    else {
      dbg_error("unknown recv queue event (%d).\n", pe.type);
    }
  }
  return HPX_SUCCESS;
}

static void _progress(transport_t *t, transport_op_t op) {
  switch (op) {
  case TRANSPORT_FLUSH:
    _send_flush(t);
    break;
  case TRANSPORT_POLL:
    hpx_addr_t and = hpx_lco_and_new(2);
    hpx_call(HPX_HERE, _send_progress_action, and, &t, sizeof(t));
    hpx_call(HPX_HERE, _recv_progress_action, and, &t, sizeof(t));
    hpx_lco_wait(and);
    hpx_lco_delete(and, HPX_NULL);
    break;
  case TRANSPORT_CANCEL:
    break;
  default:
    break;
  }
}

static uint32_t _get_send_limit(transport_t *t) {
  return t->send_limit;
}

static uint32_t _get_recv_limit(transport_t *t) {
  return t->recv_limit;
}


transport_t *transport_new_portals(uint32_t send_limit, uint32_t recv_limit) {
  if (boot_type(here->boot) != HPX_BOOT_PMI) {
    dbg_error("Portals transport unsupported with non-PMI bootstrap.\n");
  }

  portals_t *portals = malloc(sizeof(*portals));

  portals->class.type           = HPX_TRANSPORT_PORTALS;
  portals->class.id             = _id;
  portals->class.barrier        = _barrier;
  portals->class.request_size   = _request_size;
  portals->class.rkey_size      = _rkey_size;
  portals->class.request_cancel = _request_cancel;
  portals->class.adjust_size    = _adjust_size;
  portals->class.get_send_limit = _get_send_limit;
  portals->class.get_recv_limit = _get_recv_limit;

  portals->class.delete     = _delete;
  portals->class.pin        = _pin;
  portals->class.unpin      = _unpin;
  portals->class.put        = _put;
  portals->class.get        = _get;
  portals->class.send       = _send;
  portals->class.probe      = _probe;
  portals->class.recv       = _recv;
  portals->class.test       = _test;
  portals->class.testsome   = NULL;
  portals->class.progress   = _progress;
  portals->class.send_limit = (send_limit == 0) ? portals_default_srlimit : send_limit;
  portals->class.send_limit = (recv_limit == 0) ? portals_default_srlimit : recv_limit;
  portals->class.rkey_table = NULL;

  portals->interface            = PTL_INVALID_HANDLE;
  portals->sendq                = PTL_INVALID_HANDLE;
  portals->recvq                = PTL_INVALID_HANDLE;
  portals->pte                  = PTL_PT_ANY;
  portals->bufdesc              = PTL_INVALID_HANDLE;

  LIBHPX_REGISTER_ACTION(_send_progress, &_send_progress_action);
  LIBHPX_REGISTER_ACTION(_recv_progress, &_recv_progress_action);

  _portals_init(portals);
  _set_map(portals);
  _setup_portals(portals);

  return &portals->class;
}
