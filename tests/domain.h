#ifndef DOMAIN_HPX_H
#define DOMAIN_HPX_H

typedef struct {
  int nDoms;
  int maxCycles;
  int cores;
} main_args_t;

typedef struct Domain {
  hpx_addr_t complete;
  hpx_addr_t newdt;
  int nDoms;
  int rank;
  int maxcycles;
  int cycle;
} Domain;

typedef struct {
  int           index;
  int           nDoms;
  int       maxcycles;
  int           cores;
  hpx_addr_t complete;
  hpx_addr_t newdt;
} InitArgs;

#define BUFFER_SIZE 128

typedef struct InitBuffer {
  int  index;
  char message[BUFFER_SIZE];
} InitBuffer;

typedef struct contTest_config {
  long arraySize;   // Global array size
  hpx_addr_t array; // Global address of the array
}contTest_config_t;

#endif

