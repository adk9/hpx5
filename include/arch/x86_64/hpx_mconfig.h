
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Machine Configuration Function Definitions
  hpx_mconfig.h

  Copyright (c) 2013,      Trustees of Indiana University 
  Copyright (c) 2002-2012, Intel Corporation
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/


#pragma once
#ifndef LIBHPX_MCONFIG_X86_64_H_
#define LIBHPX_MCONFIG_X86_64_H_

#include <stdint.h>


/* CPU Feature Flags */
#define HPX_MCONFIG_CPU_HAS_FXSR                                0x01


/*
 --------------------------------------------------------------------
  Machine Configuration Data
 --------------------------------------------------------------------
*/

typedef struct {
  uint32_t _cpu_flags1;               /* ECX after CPUID, EAX = 1 */
  uint32_t _cpu_flags2;               /* EDX after CPUID, EAX = 1 */
} hpx_mconfig_t;


/*
 --------------------------------------------------------------------
  Machine Configuration Functions
 --------------------------------------------------------------------
*/

void hpx_mconfig_init(hpx_mconfig_t *);
int hpx_mconfig_cpu_has_fxsr(hpx_mconfig_t *);

#define HPX_MCONFIG_X86_64_SAVE_XMM                             0x01

#endif
