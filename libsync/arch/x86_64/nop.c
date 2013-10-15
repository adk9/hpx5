#include "nop.h"

void hpx_sync_nop() {
    __asm__ volatile ("nop");
}
