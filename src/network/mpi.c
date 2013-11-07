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

#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#ifdef __linux__
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <mpi.h>

#include "bootstrap/bootstrap.h"
#include "network/network.h"
#include "hpx/action.h"
#include "hpx/init.h"
#include "hpx/parcel.h"

#include "network_mpi.h"

int _argc;
char **_argv;
char *_argv_buffer;

/* MPI network operations */
network_ops_t mpi_ops = {
    .init     = init_mpi,
    .finalize = finalize_mpi,
    .progress = progress_mpi,
    .probe    = probe_mpi,
    .send     = send_mpi,
    .recv     = recv_mpi,
    .sendrecv_test     = test_mpi,
    .send_test     = test_mpi,
    .recv_test     = test_mpi,
    .put      = put_mpi,
    .get      = get_mpi,
    .putget_test = test_mpi,
    .pin      = pin_mpi,
    .unpin    = unpin_mpi,
    .phys_addr= phys_addr_mpi,
};

int eager_threshold_mpi = EAGER_THRESHOLD_MPI_DEFAULT;
int _rank_mpi;
int _size_mpi;

int init_mpi(void) {
  int retval;
  int temp;
  int thread_support_provided;

  retval = HPX_ERROR;

#if 0
  /* Get argc and argv */
  /* BDM: TODO: Move to utils. Or throw out. MPI_Init can be given
     NULL, NULL so this isn't strictly necessary. */
#if __linux__
  /* TODO: find way to do this when NOT on Linux, since /proc is Linux-specific */

  /* Procedure:
     1a. Find out how much space we need
     1b. Allocate space
     2. Copy command line from proc
     3. Parse command line into _argv so we can pass it to MPI_Init
  */
  bool fallback;
  long arg_max;
  int _argv_len;

  int cmdline_fd;
  pid_t pid;
  char filename[256];

  char prev, curr;
  int i, j;

  fallback = false;

  /* find how much space we need, then allocate buffer to hold copy of argv */
  _argv_len = 0;
  pid = getpid();
  arg_max = sysconf(_SC_ARG_MAX);
  if (arg_max <= 0) { /* can supposedly only happen if _SC_ARG_MAX is undefined */
    fallback = true;
    goto fail;
  }
  _argv_buffer = hpx_alloc(arg_max);
  if (_argv_buffer == NULL) {
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;
    goto error;
  }

  /* copy command line from proc */
  snprintf(filename, 255, "/proc/%d/cmdline", (int)pid);
  cmdline_fd = open(filename, O_RDONLY);
  if (cmdline_fd < 0) {
    fallback = true;
    goto read_fail;
  }
  /* TODO: check reason for error */
  _argv_len = read(cmdline_fd, (void*)(_argv_buffer + _argv_len), arg_max - _argv_len);
  if (_argv_len < 0) {
    fallback = true;
    goto read_fail;
  }
  close(cmdline_fd);

  /* parse raw command line into _argv */
  if (_argv_len == 0)
    exit(-1);
  _argv = hpx_alloc(sizeof(char*)*(_argv_len>>2)); // can't possibly have more arguments than this
  if (_argv == NULL) {
    free(_argv_buffer);
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;
    goto error;
  }
  
  _argv[0] = &_argv_buffer[0];
  _argc = 0;
  prev = 0;
  curr = _argv_buffer[0];
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

 read_fail:
  free(_argv_buffer);
 fail:
  if (fallback) {
    _argc = 0;
    _argv = NULL;
  }
  MPI_Initialized(&retval);
  if (!retval)
    //  temp = MPI_Init_thread(&_argc, &_argv, MPI_THREAD_MULTIPLE, &thread_support_provided);
    temp = MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &thread_support_provided);
#else
  MPI_Initialized(&retval);
  if (!retval)
    temp = MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &thread_support_provided);
#endif // ifdef __linux__
#endif //if 0   

  MPI_Initialized(&retval);
  if (!retval) {
    temp = MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &thread_support_provided);
    if (temp == MPI_SUCCESS)
      retval = 0;
    else
      __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
  #if DEBUG
  printf("thread_support_provided = %d\n", thread_support_provided);
  #endif
  }
  else
    retval = HPX_SUCCESS;

  /* cache size and rank */
  _rank_mpi = bootmgr->get_rank();
  _size_mpi = bootmgr->size();

  return retval;
}

int send_parcel_mpi(hpx_locality_t * loc, hpx_parcel_t * parc) {
  /* pseudocode:
     if size > eager_threshold:
       send notice to other process of intent to put via rdma
       put data via rdma
     else:
       send parcel using _send_mpi
  */
  return HPX_ERROR;
}

/* status may NOT be NULL */
int probe_mpi(int source, int* flag, network_status_t* status) {
  int retval;
  int temp;
  int mpi_src;
  /* int mpi_len; LD:unused */

  retval = HPX_ERROR;
  if (source == NETWORK_ANY_SOURCE)
    mpi_src = MPI_ANY_SOURCE;

  temp = MPI_Iprobe(mpi_src, MPI_ANY_TAG, MPI_COMM_WORLD, flag, &(status->mpi));

  if (temp == MPI_SUCCESS) {
    retval = 0;
    if (*flag == true) {
      status->source = status->mpi.MPI_SOURCE;
      MPI_Get_count(&(status->mpi), MPI_BYTE, &(status->count));
    }
  }
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

/* Send data via MPI. Presumably this will be an "eager" send. Don't use "data" until it's done! */
int send_mpi(int dest, void *data, size_t len, network_request_t *request) {
  int retval;
  int temp;

  retval = HPX_ERROR;

  /* TODO: move this when checking for eager_threshhold boundary */
  if (len > INT_MAX) {
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;
  }
  /* not necessary because of eager_threshold */
#if 0
  /* TODO: put this back in - but maybe make this automatically call put() in place of the send */
  if (len > eager_threshold_mpi) { /* need to use _network_put_* for that */
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;    
  }
#endif

  temp = MPI_Isend(data, (int)len, MPI_BYTE, dest, _rank_mpi, MPI_COMM_WORLD, &(request->mpi));

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

/* this is non-blocking recv - user must test/wait on the request */
int recv_mpi(int source, void* buffer, size_t len, network_request_t *request) {
  int retval;
  int temp;
  int tag;
  int mpi_src;
  int mpi_len;

  retval = HPX_ERROR;
  if (source == NETWORK_ANY_SOURCE) {
	  mpi_src = MPI_ANY_SOURCE;
	  tag = 0;
  }
  else {
	  mpi_src = source;
	  tag = source;
  }
  
  /* This may go away eventually. If we take this out, we need to use MPI_Get_count to get the size (which introduces problems with threading, should we ever change that) or we need to change sending to send the size first. */
  if (len == NETWORK_ANY_LENGTH)
    mpi_len = eager_threshold_mpi;
  else {
    if (len > eager_threshold_mpi) { /* need to use _network_put_* for that */
      __hpx_errno = HPX_ERROR;
      retval = HPX_ERROR;    
      goto error;
    }
    else
      mpi_len = len;
  }

  temp = MPI_Irecv(buffer, (int)mpi_len, MPI_BYTE, mpi_src, tag, MPI_COMM_WORLD, &(request->mpi));

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

 error:
  return retval;  
}

/* status may be NULL */
int test_mpi(network_request_t *request, int *flag, network_status_t *status) {
  int retval;
  int temp;
  retval = HPX_ERROR;

  if (status == NULL)
    temp = MPI_Test(&(request->mpi), flag, MPI_STATUS_IGNORE);
  else
    temp = MPI_Test(&(request->mpi), flag, &(status->mpi));

  if (temp == MPI_SUCCESS) {
    retval = 0;
    if (*flag == true)
      status->source = status->mpi.MPI_SOURCE;
  }
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;  
}

int put_mpi(int dest, void *buffer, size_t len, network_request_t *request) {
  return HPX_ERROR;
}

int get_mpi(int src, void *buffer, size_t len, network_request_t *request) {
  return HPX_ERROR;
}

/* Return the physical network ID of the current process */
int phys_addr_mpi(hpx_locality_t *l) {
  int ret;
  ret = HPX_ERROR;

  if (!l) {
    /* TODO: replace with more specific error */
    __hpx_errno = HPX_ERROR;
    return ret;
  }

  l->rank = bootmgr->get_rank();
  return 0;
}

void progress_mpi(void *data) {
}

int finalize_mpi(void) {
  int retval;
  int temp;
  retval = HPX_ERROR;

  MPI_Finalized(&retval);
  if (!retval) {
    temp = MPI_Finalize();

    if (temp == MPI_SUCCESS)
      retval = 0;
    else
      __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
  }

  free(_argv_buffer);
  free(_argv);

  return retval;
}

int pin_mpi(void* buffer, size_t len) {
  return 0;
}

int unpin_mpi(void* buffer, size_t len) {
  return 0;
}

