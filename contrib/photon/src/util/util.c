#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "util.h"

#ifdef DEBUG
time_t _tictoc(time_t stime, int proc) {
  time_t etime;
  etime = time(NULL);
  if ((etime - stime) > 10) {
    if( proc >= 0 )
      fprintf(stderr, "Still waiting for a recv buffer from %d", proc);
    else
      fprintf(stderr, "Still waiting for a recv buffer from any peer");
    stime = etime;
  }
  return stime;
}
#endif

void photon_gettime_(double *s) {

  struct timeval tp;

  gettimeofday(&tp, NULL);
  *s = ((double)tp.tv_sec) + ( ((double)tp.tv_usec) / 1000000.0);
  return;
}

const char *photon_addr_getstr(photon_addr *addr, int af) {
  char *buf = malloc(40);
  return inet_ntop(AF_INET6, addr->raw, buf, 40);
}
  
