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
#ifndef LIBHPX_RUNTIME_H_
#define LIBHPX_RUNTIME_H_

#include "hpx/types.h"

typedef struct hpx_locality_t {
    char *hostname;
    uint32 rank;
    union {
        uint32 nid;
        uint32 pid;
    } physical;
} hpx_locality_t;

hpx_locality_t* hpx_locality_create(void);

void hpx_locality_destroy(hpx_locality_t*);

hpx_locality_t *hpx_get_my_locality(void);

hpx_locality_t *hpx_get_locality(int rank);

uint32 hpx_get_num_localities(void);

uint32 hpx_get_rank(void);

#endif
