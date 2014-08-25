#ifndef DOMAIN_HPX_H
#define DOMAIN_HPX_H

typedef struct Domain {
  int nDoms;
  int rank;
  int maxcycles;
} Domain;

typedef struct {
  int           index;
  int           nDoms;
  int       maxcycles;
  int           cores;
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

