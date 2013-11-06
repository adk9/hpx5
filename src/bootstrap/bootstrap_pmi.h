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
#ifndef LIBHPX_BOOTSTRAP_PMI_H_
#define LIBHPX_BOOTSTRAP_PMI_H_

#include <stdlib.h>

#include "bootstrap.h"
struct hpx_locality;
extern bootstrap_ops_t pmi_boot_ops;

/**
 * pmirun bootstrap operations
 */

int bootstrap_pmi_init(void);
int bootstrap_pmi_get_rank(void);
int bootstrap_pmi_get_addr(struct hpx_locality *);
int bootstrap_pmi_size(void);
int bootstrap_pmi_get_map(struct hpx_locality **);
int bootstrap_pmi_finalize(void);

#endif /* LIBHPX_BOOTSTRAP_PMI_H_ */
