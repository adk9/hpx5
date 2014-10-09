#ifndef UTILITY_FUNCTIONS
#define UTILITY_FUNCTIONS

#include <inttypes.h>
#include "gni_pub.h"

unsigned int get_gni_nic_address(int device_id);

uint32_t get_cookie(void);
uint8_t get_ptag(void);

int get_cq_event(gni_cq_handle_t cq_handle, unsigned int source_cq, unsigned int retry, gni_cq_entry_t *next_event);


#endif
