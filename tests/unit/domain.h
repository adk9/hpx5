#ifndef DOMAIN_HPX_H
#define DOMAIN_HPX_H

typedef struct {
  int           index;
  int           nDoms;
  int       maxcycles;
  int           cores;
  hpx_addr_t complete;
  hpx_addr_t newdt;
} InitArgs;

#define BUFFER_SIZE 128

typedef struct initBuffer {
  int  index;
  char message[BUFFER_SIZE];
}initBuffer_t;

typedef struct {
  size_t n;
  hpx_addr_t addr;
  hpx_addr_t cont;
} _memget_args_t;

typedef struct contTest_config {
  long arraySize;   // Global array size
  hpx_addr_t array; // Global address of the array
}contTest_config_t;

#endif

