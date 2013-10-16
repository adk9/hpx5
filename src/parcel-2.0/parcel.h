#pragma once

/**
 * The hpx_parcel structure is what is visible at the user-level (@see
 * hpx_thread).
 */
struct hpx_parcel {
  void         *data;
  hpx_action_t  action;
  hpx_address_t address;
};
