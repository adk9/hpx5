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

static hpx_action_t _main = 0;
static hpx_action_t _init_sources        = 0;

static void _register_actions(void);
int _init_sources_action(void);

#endif
