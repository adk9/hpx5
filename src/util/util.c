#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

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
