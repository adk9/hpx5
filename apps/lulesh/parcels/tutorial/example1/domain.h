#ifndef DOMAIN_H
#define DOMAIN_H

typedef struct {
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


#endif
