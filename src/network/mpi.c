/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  MPI Network Interface 
  mpi.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <stdlib.h>

#ifdef __linux__
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <mpi.h>

#include "hpx/action.h"
#include "hpx/network.h"
#include "hpx/parcel.h"
#include "hpx/network/mpi.h"

int _argc;
char **_argv;
char *_argv_buffer;

/* MPI network operations */
network_ops_t mpi_ops = {
    .init     = _init_mpi,
    .finalize = _finalize_mpi,
    .progress = _progress_mpi,
    .send     = _send_mpi,
    .recv     = _recv_mpi,
    .sendrecv_test = _test_mpi,
    .put      = _put_mpi,
    .get      = _get_mpi,
    .putget_test = _test_mpi,
};

int _eager_threshold_mpi = _EAGER_THRESHOLD_MPI_DEFAULT;

int _init_mpi(void) {
  int retval;
  int temp;
  int thread_support_provided;

  retval = HPX_ERROR;

#if __linux__
  /* TODO: find way to do this when NOT on Linux, since /proc is Linux-specific */
  int cmdline_fd;
  ssize_t cmdline_bytes_read;
  long arg_max;
  int _argv_len;
  pid_t pid;
  char filename[256];

  //  filename = hpx_alloc(sizeof(char)*256);

  _argv_len = 0;
  pid = getpid();
  arg_max = sysconf(_SC_ARG_MAX);
  if (arg_max <= 0)
    exit(-1);


  snprintf(filename, 255, "/proc/%d/cmdline", (int)pid);

  printf("We are ok so far!\n");
  printf("arg_max = %ld\n", (long)arg_max);

  _argv_buffer = hpx_alloc(arg_max);
  printf("_argv_buffer = %zx\n", (size_t)_argv_buffer);

  printf("Getting %s ...\n", filename);
  cmdline_fd = open(filename, O_RDONLY);
  do {
    cmdline_bytes_read = read(cmdline_fd, (void*)(_argv_buffer + _argv_len), arg_max - _argv_len);
    _argv_len += cmdline_bytes_read;
  } while (cmdline_bytes_read != 0);
  close(cmdline_fd);

  if (_argv_len == 0)
    exit(-1);
  _argv = hpx_alloc(sizeof(char*)*(_argv_len>>2)); // can't possibly have more arguments than this
  _argv[0] = &_argv_buffer[0];
  _argc = 0;
  char prev, curr;
  prev = 0;
  curr = _argv_buffer[0];
  int i, j;
  i = 0;
  j = 0;
  while (!(curr == 0 && prev == 0) && (i + j < _argv_len) ) {
    _argv[_argc] = &(_argv_buffer[i]);
    do {
      prev = curr;
      curr = _argv_buffer[i+j];
      j++;
    }  while (curr != 0);
    i += j;
    j = 0;
    _argc++;

  }

#if 0
  _argv = hpx_alloc(sizeof(char*)*_argc);
  prev = 0;
  curr = _argv_buffer[0];
  i = 0;
  j = 0;
  _argv[0] = &_argv_buffer[0];
  //  while (!(curr == 0 && prev == 0) && (i + j < _argv_len) ) {
  int c;
  for(c = 0; c < _argc; c++) {
    _argv[i] = &_argv_buffer[i];
    j = 0;
    do {
      prev = curr;
      curr = _argv_buffer[i+j];
      j++;
    }  while (curr != 0);
    i += j;
    j = 0;
  }
#endif
  temp = MPI_Init_thread(&_argc, &_argv, MPI_THREAD_MULTIPLE, &thread_support_provided); /* TODO: should be argc and argv if possible */
  //  temp = MPI_Init(&_argc, &_argv); /* TODO: should be argc and argv if possible */
#else
  temp = MPI_Init_thread(0, NULL, MPI_THREAD_MULTIPLE, &thread_support_provided); /*
 TODO: should be argc and argv if possible */
#endif // ifdef __linux__

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
  
  return retval;
}

int _send_parcel_mpi(hpx_locality_t * loc, hpx_parcel_t * parc) {
  /* pseudocode:
     if size > eager_threshold:
       send notice to other process of intent to put via rdma
       put data via rdma
     else:
       send parcel using _send_mpi
  */
}

/* Send data via MPI. Presumably this will be an "eager" send. Don't use "data" until it's done! */
int _send_mpi(int dest, void *data, size_t len, network_request_t *request) {
  int retval;
  int temp;
  int rank;

  retval = HPX_ERROR;

  /*
  if (len > INT_MAX) {
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;
  }
  */ /* not necessary because of eager_threshold */
  if (len > _eager_threshold_mpi) { /* need to use _network_put_* for that */
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;    
  }

  MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* TODO: cache this, obviously */
  temp = MPI_Isend(data, (int)len, MPI_BYTE, dest, rank, MPI_COMM_WORLD, &(request->mpi));

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

/* this is non-blocking recv - user must test/wait on the request */
int _recv_mpi(int source, void* buffer, size_t len, network_request_t *request) {
  int retval;
  int temp;
  int mpi_src;
  int mpi_len;

  retval = HPX_ERROR;
  if (source == NETWORK_ANY_SOURCE)
    mpi_src = MPI_ANY_SOURCE;
  if (len == NETWORK_ANY_LENGTH)
    mpi_len = _eager_threshold_mpi;
  else {
    if (len > _eager_threshold_mpi) { /* need to use _network_put_* for that */
      __hpx_errno = HPX_ERROR;
      retval = HPX_ERROR;    
      goto error;
    }
  }

  temp = MPI_Irecv(buffer, (int)mpi_len, MPI_BYTE, mpi_src, MPI_ANY_TAG, MPI_COMM_WORLD, &(request->mpi));

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

 error:
  return retval;  
}

/* status may be NULL */
int _test_mpi(network_request_t *request, int *flag, network_status_t *status) {
  int retval;
  int temp;
  retval = HPX_ERROR;

  if (status == NULL)
    temp = MPI_Test(&(request->mpi), flag, MPI_STATUS_IGNORE);
  else
    temp = MPI_Test(&(request->mpi), flag, &(status->mpi));

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

int _put_mpi(int dest, void *buffer, size_t len, network_request_t *request) {
}

int _get_mpi(int src, void *buffer, size_t len, network_request_t *request) {
}

void _progress_mpi(void *data) {
}

int _finalize_mpi(void) {
  int retval;
  int temp;
  retval = HPX_ERROR;

  temp = MPI_Finalize();

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  free(_argv_buffer);
  free(_argv);

  return retval;
}

