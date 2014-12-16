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
#include <inttypes.h>
#include <hpx/hpx.h>

static hpx_action_t _main       = 0;
static hpx_action_t _sendData = 0;

typedef struct parcel_data {
  int index;
  char *message;
} parcel_data_t;

hpx_addr_t _partner(void) {
  int rank = hpx_get_my_rank();
  int ranks = hpx_get_num_ranks();
  return HPX_THERE((rank) ? 0 : ranks - 1);
}
 
static int _sendData_action(const parcel_data_t *args)
{
  printf("Received #%d: Message = %s\n", args->index, args->message);
  return HPX_SUCCESS;
}

static int _main_action(void *args) {
  parcel_data_t parcelData = {
    .index = hpx_get_my_rank(),
    .message = "Hello World!"
  };

  hpx_addr_t to = _partner();

  hpx_parcel_t *p = hpx_parcel_acquire(&parcelData, sizeof(parcelData));
  hpx_parcel_set_action(p, _sendData);
  hpx_parcel_set_data(p, &parcelData, sizeof(parcelData));
  hpx_parcel_set_target(p, to);

  hpx_parcel_send_sync(p);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_sendData, _sendData_action);

  return hpx_run(&_main, NULL, 0);
}
