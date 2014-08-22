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
  int  threadNo;
  char message[BUFFER_SIZE];
} InitBuffer;

#endif

