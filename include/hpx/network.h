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
#ifndef LIBHPX_NETWORK_H_
#define LIBHPX_NETWORK_H_


typedef struct network_mgr_t {
  /* List of configured transports  */
  network_trans_t *trans;
} network_mgr_t;


/**
 * Networkunication Transport
 */
typedef struct network_trans_t {
  char        name[128];
  network_ops_t *ops;
  int        *flags;
  int         active;
} network_trans_t;

/**
 * Networkunication Operations
 */
typedef struct network_ops_t {
  /* Initialize the networkunication layer */
  int (*init)(void);
  /* Send a raw payload */
  int (*send)(int peer, void *payload, size_t len);
  /* Receive a raw payload */
  int (*recv)(int peer, void *payload, size_t len);
  /* RMA put */
  int (*put)(int peer, void *dst, void *src, size_t len);
  /* RMA get */
  int (*get)(void *dst, int peer, void *src, size_t len);
  /* The networkunication progress function */
  void (*progress)(void *data);
  /* Shutdown and clean up the networkunication layer */
  void (*finalize)(void);
} network_ops_t;

/**
 * Default networkunication operations
 */

/* Initialize the networkunication layer */
int hpx_network_init(void);

/* Send a raw payload */
int hpx_network_send(int peer, void *src, size_t len);

/* Receive a raw payload */
int hpx_network_recv(int peer, void *dst, size_t len);

/* RMA put */
int hpx_network_put(int peer, void *dst, void *src, size_t len);

/* RMA get */
int hpx_network_get(void *dst, int peer, void *src, size_t len);

/* The networkunication progress function */
void hpx_network_progress(void *data);

/* Shutdown and clean up the networkunication layer */
void hpx_network_finalize(void);

#endif /* LIBHPX_NETWORK_H_ */
