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
#ifndef LIBHPX_NETWORKS_H
#define LIBHPX_NETWORKS_H


typedef struct network_status network_status_t;
typedef struct network_request network_request_t;

/// ----------------------------------------------------------------------------
/// The network interface is defined as an abstract class.
/// ----------------------------------------------------------------------------
typedef struct {
  int (*init)(void);
  void (*fini)(void);
  void (*progress)(void *data);
  int (*probe)(int src, int *flag, network_status_t *status);
  int (*send)(int dest, void *buffer, unsigned size, network_request_t *request);
  int (*recv)(int src, void *buffer, unsigned size, network_request_t *request);
  int (*test_sendrecv)(network_request_t *request, int *flag, network_status_t *status);
  int (*test_send)(network_request_t *request, int *flag, network_status_t *status);
  int (*test_recv)(network_request_t *request, int *flag, network_status_t *status);
  int (*put)(int dest, void *buffer, unsigned len, network_request_t *request);
  int (*get)(int dest, void *buffer, unsigned len, network_request_t *request);
  int (*test_putget)(network_request_t *request, int *flag, network_status_t *status);
  int (*test_put)(network_request_t *request, int *flag, network_status_t *status);
  int (*test_get)(network_request_t *request, int *flag, network_status_t *status);
  int (*pin)(void* buffer, unsigned len);
  int (*unpin)(void* buffer, unsigned len);
  int (*phys_addr)(int *locality);
  unsigned (*get_network_bytes)(unsigned n);
  void (*barrier)(void);
} network_t;

HPX_INTERNAL network_t *smp_new(void);
HPX_INTERNAL void smp_delete(network_t *smp);

HPX_INTERNAL network_t *mpi_new(void);
HPX_INTERNAL void smp_delete(network_t *mpi);

HPX_INTERNAL network_t *photon_new(void);
HPX_INTERNAL void photon_delete(network_t *photon);

#endif // LIBHPX_NETWORKS_H
