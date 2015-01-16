/* FEVS: A Functional Equivalence Verification Suite for High-Performance
 * Scientific Computing
 *
 * Copyright (C) 2007-2010, Andrew R. Siegel, Stephen F. Siegel,
 * University of Delaware
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

/* diffusion_pario.c: parallel 2d-diffusion simulation.
 * Author: Andrew R. Siegel
 * Modified by Stephen F. Siegel <siegel@cis.udel.edu>, August 2007
 * for EuroPVM/MPI 2007 tutorial
 * Modified by Guilherme Fernandes <fernande@cis.udel.edu>, December 2010
 * to use MPI I/O
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "mpi.h"

#define SQUARE(x) ((x)*(x))

/* Global variables */

int nx = -1;    /* number of discrete points including endpoints */
int ny = -1;    /* number of rows of points, including endpoints */
double k = -1;  /* D*dt/(dx*dx) */
int nsteps = -1;/* number of time steps */
int wstep = -1; /* write frame every this many time steps */
int nprocsx;    /* number of procs in the x direction */
int nprocsy;    /* number of procs in the y direction */
int nxl;        /* horizontal extent of one process; divides nprocsx */
int nyl;        /* vertical extent of one processl; divides nprocsy */
int nprocs;     /* number of processes */
int rank;       /* the rank of this process */
int xcoord;     /* the horizontal coordinate of this proc */
int ycoord;     /* the vertical coordinate of this proc */
int up;         /* rank of upper neighbor on torus */
int down;       /* rank of lower neighbor on torus */
int left;       /* rank of left neighbor on torus */
int right;      /* rank of right neighbor on torus */
double **u;     /* [nxl+2][nyl+2]; values of the discretized function */

char fileuri[256];
MPI_Datatype filetype;
MPI_File filehandle;

void quit() {
  printf("Input file must have format:\n\n");
  printf("nx = <INTEGER>\n");
  printf("ny = <INTEGER>\n");
  printf("k = <DOUBLE>\n");
  printf("nsteps = <INTEGER>\n");
  printf("wstep = <INTEGER>\n");
  printf("fileURI = <STRING[255]>\n");
  printf("<DOUBLE> <DOUBLE> ...\n\n");
  printf("where there are ny rows of nx doubles at the end.\n");
  fflush(stdout);
  exit(1);
}

void readkey(FILE *file, char *keyword) {
  char buf[101];
  int returnval;

  returnval = fscanf(file, "%100s", buf);
  if (returnval != 1)
    quit();
  if (strcmp(keyword, buf) != 0)
    quit();
  returnval = fscanf(file, "%10s", buf);
  if (returnval != 1)
    quit();
  if (strcmp("=", buf) != 0)
    quit();
}

void readstr(FILE *file, char *keyword, char *ptr) {
  readkey(file, keyword);
  printf("1\n");
  if (fscanf(file, "%255s", ptr) != 1)
    quit();
}

void readint(FILE *file, char *keyword, int *ptr) {
  readkey(file, keyword);
  if (fscanf(file, "%d", ptr) != 1)
    quit();
}

void readdouble(FILE *file, char *keyword, double *ptr) {
  readkey(file, keyword);
  if (fscanf(file, "%lf", ptr) != 1)
    quit();
}

/* init: initializes global variables. */
void init(char* infilename) {
  int i, j, z;
  int sizes[2];
  int subsizes[2];
  int starts[2];
  double *buf; /* temporary buffer */
  FILE *infile = fopen(infilename, "r");

  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (rank == 0) {
    assert(infile);
    readint(infile, "nx", &nx);
    readint(infile, "ny", &ny);
    readdouble(infile, "k", &k);
    readint(infile, "nsteps", &nsteps);
    readint(infile, "wstep", &wstep);
    readstr(infile, "fileURI", fileuri);
    printf("Diffusion2d with k=%f, nx=%d, ny=%d, nsteps=%d, wstep=%d, fileURI=%s\n",
           k, nx, ny, nsteps, wstep, fileuri);
    fflush(stdout);
  }

  MPI_Bcast(&nx, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&ny, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&k, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  MPI_Bcast(&nsteps, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&wstep, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(fileuri, 256, MPI_CHAR, 0, MPI_COMM_WORLD);

  nxl = nx / nprocsx;
  nyl = ny / nprocsy;
  assert(nprocs == nprocsx*nprocsy);
  assert(nx == nxl*nprocsx);
  assert(ny == nyl*nprocsy);

  xcoord = rank % nprocsx;
  ycoord = rank / nprocsx;
  up = xcoord + nprocsx * ((ycoord + 1) % nprocsy);
  down = xcoord + nprocsx * ((ycoord + nprocsy - 1) % nprocsy);
  left = (xcoord + nprocsx - 1) % nprocsx + nprocsx * ycoord;
  right = (xcoord + 1) % nprocsx + nprocsx * ycoord;

  u = malloc((nxl + 2) * sizeof(double *));
  assert(u);
  for (i = 0; i < nxl + 2; i++) {
    u[i] = malloc((nyl + 2) * sizeof(double*));
  }

  buf = (double*) malloc(nxl * sizeof(double));
  if (rank == 0) {
    for (i = 0; i < ny; i++) {
      for (j = 0; j < nx; j++) {
        if (fscanf(infile, "%lf", buf + (j % nxl)) != 1)
          quit();
        if (i < nyl && j < nxl) {
          u[j + 1][i + 1] = buf[j % nxl];
        }
        else if ((j+1) % nxl == 0 && (j > 0 || i != 0)) {
          MPI_Send(buf, nxl, MPI_DOUBLE, (i / nyl) * nprocsx + (j-1)
                   / nxl, 0, MPI_COMM_WORLD);
        }
      }
    }
  }
  else {
    for (i = 1; i <= nyl; i++) {
      MPI_Recv(buf, nxl, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
      for (j = 1; j <= nxl; j++)
        u[j][i] = buf[j-1];
    }
  }

  sizes[0] = nx;
  sizes[1] = ny;
  subsizes[0] = nxl;
  subsizes[1] = nyl;
  starts[0] = xcoord*nxl;
  starts[1] = ycoord*nyl;

  MPI_Type_create_subarray(2, sizes, subsizes, starts, MPI_ORDER_C,
                           MPI_DOUBLE, &filetype);
  MPI_Type_commit(&filetype);

  MPI_File_open(MPI_COMM_WORLD, fileuri, MPI_MODE_CREATE | MPI_MODE_RDWR,
                MPI_INFO_NULL, &filehandle);
  MPI_File_set_view(filehandle, 0, MPI_DOUBLE, filetype, "native", MPI_INFO_NULL);
}

void write_frame(int time) {
  double buf[nxl];
  int i, j;

  if (rank != 0) {
    for (j = 1; j <= nyl; j++) {
      for (i = 1; i <= nxl; i++)
        buf[i - 1] = u[i][j];
      MPI_Send(buf, nxl, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
    }
  }
  else {
    int k, m, n, from;
    char filename[50];
    FILE *file = NULL;

    sprintf(filename, "./parioout/out_%d", time);
    file = fopen(filename, "w");
    assert(file);
    for (n = 0; n < nprocsy; n++) {
      for (j = 1; j <= nyl; j++) {
        for (m = 0; m < nprocsx; m++) {
          from = n * nprocsx + m;
          if (from != 0)
            MPI_Recv(buf, nxl, MPI_DOUBLE, from, 0, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
          else
            for (i = 1; i <= nxl; i++)
              buf[i - 1] = u[i][j];
          for (i = 1; i <= nxl; i++)
            fprintf(file, "%8.2f", buf[i - 1]);
        }
        fprintf(file, "\n");
      }
    }
    fclose(file);
  }
}

/* read_frame: reads current values of u from file */
void read_frame() {
  double buf[nxl*nyl];
  int i, j;

  MPI_File_read_all(filehandle, buf, nxl*nyl, MPI_DOUBLE, MPI_STATUS_IGNORE);

  for (i = 0; i < nyl; i++)
    for (j = 0; j < nxl; j++)
      u[i+1][j+1] = buf[i*nxl+j];
}

/* main: executes simulation, creates one output file for each time
 * step */
int main(int argc, char *argv[]) {
  int iter;

  assert(argc==4);
  nprocsx = atoi(argv[2]);
  nprocsy = atoi(argv[3]);
  MPI_Init(&argc, &argv);
  init(argv[1]);

  for (iter = 0; iter <= nsteps; iter++) {
    if(!(iter%wstep)) {
      read_frame();
      write_frame(iter);
    }
  }

  MPI_File_close(&filehandle);
  MPI_Type_free(&filetype);
  MPI_Finalize();
  return 0;
}
