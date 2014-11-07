// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"

hpx_addr_t future;

static hpx_action_t _main = 0;
static hpx_action_t _set = 0;


static int _set_action(hpx_addr_t *args) {
  return HPX_SUCCESS;
}


static int _main_action(int *args) {
  hpx_addr_t f = hpx_lco_future_new(0);
  hpx_addr_t process = hpx_process_new(f);
  hpx_process_call(process, HPX_HERE, _set, NULL, 0, HPX_NULL);
  hpx_lco_wait(f);
  hpx_shutdown(HPX_SUCCESS);
}


int main(int argc, char * argv[argc]) {
  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  _main = HPX_REGISTER_ACTION(_main_action);
  _set = HPX_REGISTER_ACTION(_set_action);
  return hpx_run(_main, NULL, 0);
}
