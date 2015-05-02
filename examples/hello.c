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

#include <stdio.h>
#include <hpx/hpx.h>

static int _hello_action(void *args, size_t size) {
  printf("Hello World from %u.\n", hpx_get_my_rank());
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[argc]) {
  if (hpx_init(&argc, &argv) != 0)
    return -1;
  hpx_action_t hello;
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, hello, _hello_action, HPX_POINTER, HPX_SIZE_T);
  return hpx_run(&hello, NULL, 0);
}
