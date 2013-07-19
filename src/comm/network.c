/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Network Functions
  hpx_network.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Benjamin D. Martin <benjmart [at] indiana.edu>
 ====================================================================
*/

#include <mpi.h>
#include <stdint.h>

#if USE_PHOTON
#include "photon.h"

/* still have to wait on send_request via photon_wait(proc, send_request) */
int hpx_network_send_start(int proc, int tag, char* send_buffer, int size, int offset, uint32_t *send_request) {

  photon_wait_recv_buffer_rdma(proc, tag);
  photon_post_os_put(proc, send_buffer, (uint32_t)size, tag, offset, send_request);
  photon_send_FIN(proc);
  return 1;
}

int hpx_network_recv_start(int proc, int tag, char* recv_buffer, int size, int offset, uint32_t *recv_request) {
  photon_post_recv_buffer_rdma(proc, recv_buffer, (uint32_t)size, tag, recv_request);

  return 1;
}

int hpx_network_wait(uint32_t request) {
  return photon_wait(request);
}

#if 0
/* FIXME: oops this will always error in curent build of photon if it's multithreaded - which it is */
int hpx_network_waitany(int *ret_proc, int* ret_req) {
  return photon_wait_any(ret_proc, ret_req);
}
#endif

int hpx_network_test(uint32_t request, int *flag, int *type, MPI_Status *status) {
  return photon_test(request, flag, type, status);
}

int hpx_network_init(int* argc, char*** argv) {
  int rank;
  int size;
  int ret;
  ret = MPI_Init(argc,argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  if (0 == ret)
    ret =  photon_init(size, rank, MPI_COMM_WORLD);
  return ret;
}

int hpx_network_rank() {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  return rank;
}

int hpx_network_size() {
  int size;
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  return size;
}

int hpx_network_finalize() {
  int ret;
  ret = MPI_Finalize();
  if (0 == ret)
    ret = photon_finalize();
  return ret;
}

int hpx_network_register_buffer(char* buffer, int buffer_size) {
  return photon_register_buffer(buffer, buffer_size);
}

int hpx_network_unregister_buffer(char* buffer, int buffer_size) {
  return photon_unregister_buffer(buffer, buffer_size);
}
#else
int hpx_network_send_start(int proc, int tag, char* send_buffer, int size, int offset, uint32_t *send_request) { return -1; }

int hpx_network_recv_start(int proc, int tag, char* recv_buffer, int size, int offset, uint32_t *recv_request) { return -1; }

int hpx_network_wait(uint32_t request) {
  return -1;
}

int hpx_network_test(uint32_t request, int *flag, int *type, MPI_Status *status) {
  return -1;
}

int hpx_network_init(int* argc, char*** argv) {
  int rank;
  int size;
  int ret;
  ret = MPI_Init(argc,argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  return ret;
}

int hpx_network_rank() {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  return rank;
}

int hpx_network_size() {
  int size;
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  return size;
}

int hpx_network_finalize() {
  int ret;
  ret = MPI_Finalize();
  return ret;
}

int hpx_network_register_buffer(char* buffer, int buffer_size) {
  return -1;
}

int hpx_network_unregister_buffer(char* buffer, int buffer_size) {
  return -1;
}

#endif
