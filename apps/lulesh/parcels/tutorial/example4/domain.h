#ifndef DOMAIN_HPX_H
#define DOMAIN_HPX_H

typedef struct Domain {
  hpx_addr_t complete;
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
} InitArgs;


#endif
