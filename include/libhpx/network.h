/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Network Functions
  network.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#pragma once
#ifndef LIBHPX_NETWORK_H_
#define LIBHPX_NETWORK_H_

#ifdef HAVE_CONFIG_H
#include <config.h>                             /* HAVE_* */
#endif
#include <stddef.h>                             /* size_t */
#include <stdint.h>                             /* uint32_t */
#if HAVE_MPI
#include <mpi.h>
#endif
#if HAVE_PHOTON
#include <photon.h>
#endif

#include "hpx/runtime.h"                        /* hpx_locality_y */

#define NETWORK_ANY_SOURCE -1
#define NETWORK_ANY_LENGTH -1

/**
 * The network request type.
 */
struct network_request {
#if HAVE_MPI
  MPI_Request mpi;
#endif
#if HAVE_PHOTON
  uint32_t photon;
#endif
};
typedef struct network_request network_request_t;

/**
 *
 */
struct network_status {
  int source;
  int count;
#if HAVE_MPI
  MPI_Status mpi;
#endif
#if HAVE_PHOTON
  struct photon_status_t photon;
#endif
};
typedef struct network_request network_status_t;

/**
 * Network Operations interface
 *
 * This defines the abstract interface for a network implementation.
 */
struct network_ops {
  /**
   * Initialize the network layer.
   *
   * @returns HPX_SUCCESS or an error condition
   */
  int (*init)(void);

  /**
   * Teardown the network layer.
   *
   * @returns HPX_SUCCESS or an error condition
   */
  int (*finalize)(void);
  
  /**
   * The network progress function.
   *
   * @param[in] data - 
   */
  void (*progress)(void *data);

  /**
   * Probe the network layer for events.
   *
   * @param[in]    src -
   * @param[out] flags -
   * @param[in] status -
   *
   * @returns HPX_SUCCESS or an error condition
   */
  int (*probe)(int src, int* flag, network_status_t* status);

  /**
   * Send a network message.
   *
   * @param[in] dest    - the destination id
   * @param[in] buffer  - the bytes to send
   * @param[in] len     - the number of bytes
   * @param[in] request - a request identifier
   *
   * @returns HPX_SUCCESS or an error code
   */
  int (*send)(int dest, void *buffer, size_t len, network_request_t *request);

  /**
   * Receive a raw payload.
   *
   * @param[in] src     - LD: not sure
   * @param[in] buffer  - the buffer to receive into
   * @param[in] len     - the length to receive
   * @param[in] request - a request identifier
   *
   * @returns HPX_SUCCESS or an error code
   */
  int (*recv)(int src, void *buffer, size_t len, network_request_t *request);

  /**
   * Test for completion of send or receive.
   *
   * @param[in] request -
   * @param[out]   flag -
   * @param[out] status -
   *
   * @returns HPX_SUCCESS or an error code
   */
  int (*sendrecv_test)(network_request_t *request, int *flag, network_status_t *status);

  /**
   * An asynchronous RMA put operation.
   *
   * @param[in]    dest - LD: not sure
   * @param[in]  buffer - the buffer to send
   * @param[in]     len - the length to send
   * @param[in] request - a request identifier
   *
   * @returns HPX_SUCCESS or an error code
   */
  int (*put)(int dest, void *buffer, size_t len, network_request_t *request);

  /**
   * An aysnchronous RMA get operation.
   *
   * @param[in]    dest - LD: not sure
   * @param[in]  buffer - the buffer to write
   * @param[in]     len - the number of bytes
   * @param[in] request - a request identifier
   *
   * @returns HPX_SUCCESS or an error code
   */
  int (*get)(int dest, void *buffer, size_t len, network_request_t *request); 

  /**
   * Test for completion of communication.
   *
   * @param[in] request - the request to check
   * @param[out]   flag - LD: not sure
   * @param[out] status - LD: not sure
   *
   * @returns HPX_SUCCESS or an error code
   */
  int (*putget_test)(network_request_t *request, int *flag, network_status_t *status);

  /**
   * Pin memory for put() or get().
   *
   * @param[in] buffer - buffer address
   * @param[in]    len - the extent to pin
   *
   * @return HPX_SUCCESS or an error code
   */
  int (*pin)(void* buffer, size_t len);

  /**
   * Unpin memory after a put() or get().
   *
   * @param[in] buffer - the buffer address
   * @param[in]    len - the extent
   */
  int (*unpin)(void* buffer, size_t len);

  /**
   * Get the physical address of the current locality.
   *
   * @param[out] locality - the locality
   *
   * @returns HPX_SUCCESS or an error code
   */
  int (*phys_addr)(hpx_locality_t *locality);

  /**
   * Adjust the number of bytes for a network operation.
   *
   * Some networks have strict requirements on the size of messages. For
   * instance, the message length for ugni appears to be four bytes. This
   * encapsulates the network-specific functionality.
   *
   * @param[in] n - number of bytes we want to send
   *
   * @returns the number of bytes that the network requires that we send.
   */
  size_t (*get_network_bytes)(size_t n);

  /**
   * Network-specific barrier implementation.
   */
  void (*barrier)(void);
};
typedef struct network_ops network_ops_t;

/** The concrete implementations of the network operations. @{ */
extern network_ops_t default_net_ops;
extern network_ops_t mpi_ops;
extern network_ops_t photon_ops;
extern network_ops_t sim_ops;
/** @} */

#endif /* LIBHPX_NETWORK_H_ */

