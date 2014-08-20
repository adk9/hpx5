#ifndef COMMON_H
#define COMMON_H

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <hpx/hpx.h>

typedef struct {
  int nDoms;
  int maxCycles;
  int cores;
} main_args_t;

int unittest_main_action(const main_args_t *args);

void unittest_init_actions(void);

#endif
