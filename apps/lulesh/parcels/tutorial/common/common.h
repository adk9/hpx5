#ifndef COMMON_H
#define COMMON_H

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include "hpx/hpx.h"

typedef struct {
  int nDoms;
  int maxCycles;
  int cores;
} main_args_t;

int main_action(const main_args_t *args);

void parse_command_line(int argc, char * const argv[argc],
                        hpx_config_t *config, main_args_t *args);

#endif
