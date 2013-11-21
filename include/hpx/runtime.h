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
#ifndef HPX_RUNTIME_H_
#define HPX_RUNTIME_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct hpx_locality hpx_locality_t;

/* LD: this entire file needs to be documented. Also, why isn't there a
 * locality.h file that is seperate? */

struct hpx_locality {
  /* char *hostname; */
  /* BDM: If we put this in, we need to change parcel serialization. Since this
     is referenced nowhere else, I've taken it out for now. Do we need this? */ 
    uint32_t rank;
    union {
        uint32_t nid;
        uint32_t pid;
    } physical;
};

hpx_locality_t *hpx_locality_create(void);
void            hpx_locality_destroy(hpx_locality_t *);
hpx_locality_t *hpx_locality_from_rank(int);
hpx_locality_t *hpx_get_my_locality(void);
hpx_locality_t *hpx_find_locality(int rank);
uint32_t        hpx_get_num_localities(void);
uint32_t        hpx_get_rank(void);
bool            hpx_locality_equal(const hpx_locality_t *lhs, const hpx_locality_t *rhs);

#endif /* HPX_RUNTIME_H_ */
