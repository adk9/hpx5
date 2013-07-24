/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Thread Function Definitions
  hpx_thread.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#pragma once
#ifndef _HPX_H_
#define _HPX_H_

#include <stdarg.h>
#include <stdint.h>

#define _HPX_H_INSIDE

// TODO: Cut down on the number of internal headers.

#include "hpx/action.h"
#include "hpx/agas.h"
#include "hpx/atomic.h"
#include "hpx/config.h"
#include "hpx/ctx.h"
#include "hpx/error.h"
#include "hpx/heap.h"
#include "hpx/init.h"
#include "hpx/kthread.h"
#include "hpx/lco.h"
#include "hpx/list.h"
#include "hpx/map.h"
#include "hpx/mctx.h"
#include "hpx/mem.h"
#include "hpx/network.h"
#include "hpx/parcel.h"
#include "hpx/parcelhandler.h"
#include "hpx/queue.h"
#include "hpx/runtime.h"
#include "hpx/thread.h"
#include "hpx/timer.h"
#include "hpx/types.h"

#undef _HPX_H_INSIDE

#endif /* _HPX_H */
