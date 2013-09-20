
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Exports
  hpx.h

  Copyright (c) 2013, Trustees of Indiana University 
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
#ifndef LIBHPX_H_
#define LIBHPX_H_

#include "hpx/init.h"
#include "hpx/ctx.h"
#include "hpx/list.h"
#include "hpx/queue.h"
#include "hpx/thread.h"

#ifdef __x86_64__
#include "hpx/arch/x86_64/mconfig_defs.h"
#include "hpx/arch/x86_64/mconfig.h"
#include "hpx/arch/x86_64/mregs.h"
#endif

#endif /* LIBHPX_H */
