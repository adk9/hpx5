#include "nop.h"

void sync_nop() {
    __asm__ volatile ("nop");
}
