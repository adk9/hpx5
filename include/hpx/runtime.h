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

typedef struct hpx_locality_t {
    char *hostname;
    uint32 rank;
    union {
        uint32 nid;
        uint32 pid;
    } physical;
} hpx_locality_t;

int hpx_create_locality(hpx_locality_t *);

int hpx_get_locality(hpx_locality_t *);
