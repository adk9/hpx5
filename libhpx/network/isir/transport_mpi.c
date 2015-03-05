// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <mpi.h>
#include <libhpx/debug.h>
#include "transport.h"

typedef struct {
  hpx_transport_t type;
} mpi_isir_transport_t;

static void _init_mpi(void) {
  int init = 0;
  MPI_Initialized(&init);
  if (!init) {
    static const int LIBHPX_THREAD_LEVEL = MPI_THREAD_FUNNELED;
    int level;
    int e = MPI_Init_thread(NULL, NULL, LIBHPX_THREAD_LEVEL, &level);
    if (e != MPI_SUCCESS) {
      dbg_error("mpi initialization failed\n");
    }

    if (level != LIBHPX_THREAD_LEVEL) {
      log_error("MPI thread level failed requested %d, received %d.\n",
                LIBHPX_THREAD_LEVEL, level);
    }

    log_trans("thread_support_provided = %d\n", level);
  }
}

void *isir_transport_new_mpi(const config_t *cfg) {
  _init_mpi();

  mpi_isir_transport_t *mpi = malloc(sizeof(*mpi));
  mpi->type = HPX_TRANSPORT_MPI;
  return mpi;
}
