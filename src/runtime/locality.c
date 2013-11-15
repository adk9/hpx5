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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdint.h>
#include <string.h>                             /* memcpy */
#include <strings.h>                            /* bzero */

#include "hpx/error.h"                          /* __hpx_errno */
#include "hpx/globals.h"                        /* __hpx_network_ops */
#include "hpx/mem.h"                            /* hpx_alloc/free */
#include "hpx/runtime.h"                        /* struct hpx_locality */
#include "bootstrap.h"                          /* struct bootmgr */
#include "debug.h"
#include "network.h"                            /* struct hpx_network_ops */

/* LD: The initialization of this is not thread-safe. This seems dangerous
 * because it's initialized lazily in hpx_get_my_locality(), but is ok if we
 * know that the first instance of this happens sequentially.
 *
 * It's possibly also ok if the hpx_locality_create() call is idempotent, then
 * we just leak some memory.
 *
 * Ultimately, if this needs to become thread safe then we have a bit of a
 * problem, since the constructor calls out to the __hpx_network_ops.
 */
static hpx_locality_t *my_locality = NULL;

hpx_locality_t *
hpx_locality_create(void)
{
  hpx_locality_t *loc = hpx_alloc(sizeof(*loc));
  if (!loc)
    dbg_print_error(HPX_ERROR, "Failed to allocate a locality");
  bzero(loc, sizeof(*loc));
  return loc;
}

void
hpx_locality_destroy(hpx_locality_t* loc)
{
  hpx_free(loc);
}

hpx_locality_t *
hpx_get_my_locality(void)
{
  if (!my_locality) {
    my_locality = hpx_locality_create();
    dbg_assert(my_locality && "Failed to create a locality");
    /* TODO: replace with real runtime configured rank setting */
    __hpx_network_ops->phys_addr(my_locality);
  }
  return my_locality;
}

hpx_locality_t *
hpx_locality_from_rank(int rank)
{
  hpx_locality_t *l = hpx_locality_create();
  if (l)
    l->rank = rank;
  return l;                                     /* LD: error? */
}

hpx_locality_t *
hpx_find_locality(int rank)
{
  hpx_locality_t *l = hpx_locality_create();
  if (!l)
    return NULL;

  hpx_locality_t *locs = NULL;
  if (bootmgr->get_map(&locs))
    return NULL;
  
  hpx_locality_t *m = &locs[rank];
  if (!m)
    memcpy(l, m, sizeof(*l));

  free(locs);                                   /* LD: free vs hpx_free? */
  return l;
}

uint32_t
hpx_get_num_localities(void)
{
  // ask the network layer for the number of localities
  /* TODO: replace with real runtime configured ranks */
  return (uint32_t)bootmgr->size();
}

uint32_t
hpx_get_rank(void)
{
  return hpx_get_my_locality()->rank;
}

bool
hpx_locality_equal(const hpx_locality_t *lhs, const hpx_locality_t *rhs)
{
  return (lhs->rank == lhs->rank) && (lhs->physical.nid == rhs->physical.nid);
}
