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
#ifndef LIBHPX_BOOTSTRAP_H_
#define LIBHPX_BOOTSTRAP_H_

#include "hpx/network.h"

typedef struct bootstrap_ops_t {
  /* Initialize the bootstrapping module */
  int (*init)(void);
  /* Shutdown and clean up the bootstrap module */
  int (*shutdown)(void);
  /* Get a unique identifier */
  void (*id)(void *data);
  /* Get  */
  int (*size)(void);
} bootstrap_ops_t;



#endif /* LIBHPX_BOOTSTRAP_H_ */
