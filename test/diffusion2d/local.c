/*
 * Local: each node writes its data to the local FS on a separate file.
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "d2dmodule.h"

static double *buf;

void module_init() {
  char myfile[256];

  snprintf(myfile, 256, "%s_%d", fileuri, rank);
  outfile = fopen(myfile, "w+");
  assert(outfile);

  buf = malloc(nxl*nyl*sizeof(double));
  assert(buf);
}

void pre_exchange(int io_frame) {
}

/* write_frame: writes current values of u to file */
void write_frame(int time) {
  int wcount = nxl*nyl;
  int i, j;

  for (i = 0; i < nyl; i++)
    for (j = 0; j < nxl; j++)
      buf[i*nxl+j] = u[j+1][i+1];

  if (fwrite(buf, sizeof(double), wcount, outfile) != wcount) {
    perror("fwrite");
    exit(-1);
  }
}

void module_finalize() {
  fclose(outfile);
}

