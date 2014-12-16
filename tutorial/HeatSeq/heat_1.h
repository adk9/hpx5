#include <limits.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "hpx/hpx.h"

#define N 30

/* Grid declarations to be shared and blocked as the biggest chunks of rows */
typedef struct grid_point {
  double grid[N+2][N+2];
  double new_grid[N+2][N+2];
}grid_point_t;

typedef struct {
  grid_point_t *array; // Storage for zeroth and first levels
}grid_storage_t;

typedef struct {
  int index;
  int nDoms;
  int maxcycles;
  int cores;
} InitArgs;

typedef struct Domain {
  int nDoms;
  int myIndex;
  grid_storage_t *grid_points;
} Domain;
