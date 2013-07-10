/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  The HPX 5 Runtime (Locality operations)
  locality.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <stdlib.h>

#include "hpx/action.h"
#include "hpx/agas.h"
#include "hpx/parcel.h"
#include "hpx/runtime.h"

int hpx_create_locality(hpx_locality_t *) {
}

hpx_locality_t *hpx_get_my_locality(void) {
}

hpx_locality_t *hpx_get_locality(int rank) {
}

uint32 hpx_get_num_localities(void) {
}

uint32 hpx_get_rank(void) {
}
