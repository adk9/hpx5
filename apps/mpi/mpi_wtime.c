#include <hpx/hpx.h>
#include <stdio.h>
#include <string.h>

hpx_time_t start_time;

double mpi_wtime(void) {
  return hpx_time_elapsed_ms(start_time);
}
