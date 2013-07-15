/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_COMM_H_
#define LIBHPX_COMM_H_

/**
 * Communication Operations
 */

typedef struct comm_operations_t {
    /* Initialize the communication layer */
    int (*init)(void);
    /* Helper to send a parcel structure */
    int (*send_parcel)(hpx_locality_t *, hpx_parcel_t *);
    /* Send a raw payload */
    int (*send)(int peer, void *payload, size_t len);
    /* RMA put */
    int (*put)(int peer, void *dst, void *src, size_t len);
    /* RMA get */
    int (*get)(void *dst, int peer, void *src, size_t len);
    /* The communication progress function */
    void (*progress)(void *data);
    /* Shutdown and clean up the communication layer */
    void (*finalize)(void);
} comm_operations_t;

/**
 * Default communication operations
 */

/* Initialize the communication layer */
int _comm_init(void);

/* Helper to send a parcel structure */
int _comm_send_parcel(hpx_locality_t *, hpx_parcel_t *);

/* Send a raw payload */
int _comm_send(int peer, void *payload, size_t len);

/* RMA put */
int _comm_put(int peer, void *dst, void *src, size_t len);

/* RMA get */
int _comm_get(void *dst, int peer, void *src, size_t len);

/* The communication progress function */
void _comm_progress(void *data);

/* Shutdown and clean up the communication layer */
void _comm_finalize(void);

#endif /* LIBHPX_COMM_H_ */
