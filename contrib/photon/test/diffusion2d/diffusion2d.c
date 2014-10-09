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
 * for benchmarking MPI I/O, Photon, etc.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <mpi.h>

#include "d2dmodule.h"

#define SQUARE(x) ((x)*(x))

/* Global variables */

int nx = -1;    /* number of discrete points including endpoints */
int ny = -1;    /* number of rows of points, including endpoints */
double k = -1;  /* D*dt/(dx*dx) */
int nsteps = -1;/* number of time steps */
int wstep = -1; /* write frame every this many time steps */
int autogen = -1;
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
double **u_new; /* temp storage for calculating next frame */

/* Globals used by multiple modules */
char fileuri[256];
MPI_Datatype filetype;
/* MPI I/O */
MPI_File filehandle;
/* Photon */
int phorwarder;
uint32_t photon_request;
double *iobuf;
int iobuf_size;
/* Local */
FILE *outfile;

void quit() {
  printf("Input file must have format:\n\n");
  printf("nx = <INTEGER>\n");
  printf("ny = <INTEGER>\n");
  printf("k = <DOUBLE>\n");
  printf("nsteps = <INTEGER>\n");
  printf("wstep = <INTEGER>\n");
  printf("fileURI = <STRING[255]>\n");
  printf("autogen = <0|1>\n");
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
    readint(infile, "autogen", &autogen);
    printf("Diffusion2d with k=%f, nx=%d, ny=%d, nsteps=%d, wstep=%d, autogen=%d, fileURI=%s\n",
           k, nx, ny, nsteps, wstep, autogen, fileuri);
    fflush(stdout);
  }

  MPI_Bcast(&nx, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&ny, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&k, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  MPI_Bcast(&nsteps, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&wstep, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&autogen, 1, MPI_INT, 0, MPI_COMM_WORLD);
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
  u_new = malloc((nxl + 2) * sizeof(double *));
  assert(u && u_new);
  for (i = 0; i < nxl + 2; i++) {
    u[i] = malloc((nyl + 2) * sizeof(double*));
    u_new[i] = malloc((nyl + 2) * sizeof(double*));
  }

  buf = (double*) malloc(nxl * sizeof(double));
  if (rank == 0) {
    for (i = 0; i < ny; i++) {
      for (j = 0; j < nx; j++) {

        if (!autogen)
          if (fscanf(infile, "%lf", buf + (j % nxl)) != 1)
            quit();
          else
            *(buf + (j % nxl)) = (!i || !j || i == ny-1 || j == nx-1) ? 0.0 : 100.0;

        if (i < nyl && j < nxl)
          u[j + 1][i + 1] = buf[j % nxl];
        else if ((j+1) % nxl == 0 && (j > 0 || i != 0))
          MPI_Send(buf, nxl, MPI_DOUBLE, (i / nyl) * nprocsx + (j-1)
                   / nxl, 0, MPI_COMM_WORLD);
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

#ifdef PROFILING
  MPI_Pcontrol(1, "d2d_module_init");
#endif
  module_init();
#ifdef PROFILING
  MPI_Pcontrol(-1, "d2d_module_init");
#endif
}

/* exchange_ghost_cells: updates ghost cells using MPI communication */
void exchange_ghost_cells() {
  int i, j;
  double sbufx[nxl], rbufx[nxl], sbufy[nyl], rbufy[nyl];

  for (i = 1; i <= nxl; ++i)
    sbufx[i - 1] = u[i][1];

  MPI_Sendrecv(sbufx, nxl, MPI_DOUBLE, down, 0, rbufx, nxl, MPI_DOUBLE, up,
               0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  for (i = 1; i <= nxl; ++i) {
    u[i][nyl + 1] = rbufx[i - 1];
    sbufx[i - 1] = u[i][nyl];
  }

  MPI_Sendrecv(sbufx, nxl, MPI_DOUBLE, up, 0, rbufx, nxl, MPI_DOUBLE, down,
               0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  for (i = 1; i <= nxl; ++i)
    u[i][0] = rbufx[i - 1];

  for (j = 1; j <= nyl; ++j)
    sbufy[j - 1] = u[1][j];

  MPI_Sendrecv(sbufy, nyl, MPI_DOUBLE, left, 0, rbufy, nyl, MPI_DOUBLE,
               right, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  for (j = 1; j <= nyl; ++j) {
    u[nxl + 1][j] = rbufy[j - 1];
    sbufy[j - 1] = u[nxl][j];
  }

  MPI_Sendrecv(sbufy, nyl, MPI_DOUBLE, right, 0, rbufy, nyl, MPI_DOUBLE,
               left, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  for (j = 1; j <= nyl; ++j)
    u[0][j] = rbufy[j - 1];
}

/* update: updates u.  Uses ghost cells.  Purely local operation. */
void update() {
  int i, j;

  for (i = 1; i <= nxl; i++) {
    for (j = 1; j <= nyl; j++) {
      if ((i == 1 && xcoord == 0) ||
          (i == nxl && (xcoord*nxl + nxl) == nx) ||
          (j == 1 && ycoord == 0) ||
          (j == nyl && (ycoord*nyl + nyl) == ny))
        u_new[i][j] = u[i][j];
      else
        u_new[i][j] = u[i][j] + k * (u[i + 1][j] + u[i - 1][j]
                                     + u[i][j + 1] + u[i][j - 1] - 4 * u[i][j]);
    }
  }
  for (i = 1; i <= nxl; i++)
    for (j = 1; j <= nyl; j++)
      u[i][j] = u_new[i][j];
}

/* main: executes simulation, creates one output file for each time
 * step */
int main(int argc, char *argv[]) {
  int iter;

  assert(argc==4);
  nprocsx = atoi(argv[2]);
  nprocsy = atoi(argv[3]);
  MPI_Init(&argc, &argv);

#ifdef PROFILING
  MPI_Pcontrol(1, "d2d_init");
#endif
  init(argv[1]);
#ifdef PROFILING
  MPI_Pcontrol(-1, "d2d_init");
#endif

  MPI_Barrier(MPI_COMM_WORLD);

#ifdef PROFILING
  MPI_Pcontrol(1, "d2d_write");
#endif
  write_frame(0);
  pre_exchange(1);
#ifdef PROFILING
  MPI_Pcontrol(-1, "d2d_write");
#endif

  for (iter = 1; iter <= nsteps; iter++) {
#ifndef IO_DISABLED
#ifdef PROFILING
    MPI_Pcontrol(1, "d2d_write");
#endif
    pre_exchange(!(iter%wstep));
#ifdef PROFILING
    MPI_Pcontrol(-1, "d2d_write");
#endif
#endif

#ifndef COMM_DISABLED
#ifdef PROFILING
    MPI_Pcontrol(1, "d2d_exchange");
#endif
    exchange_ghost_cells();
#ifdef PROFILING
    MPI_Pcontrol(-1, "d2d_exchange");
#endif
#endif

#ifndef IO_DISABLED
    if(!(iter%wstep)) {
#ifdef PROFILING
      MPI_Pcontrol(1, "d2d_write");
#endif
      write_frame(iter);
#ifdef PROFILING
      MPI_Pcontrol(-1, "d2d_update");
#endif
    }
#endif

#ifdef PROFILING
    MPI_Pcontrol(1, "d2d_update");
#endif
    update();
#ifdef PROFILING
    MPI_Pcontrol(-1, "d2d_update");
#endif
  }

#ifdef PROFILING
  MPI_Pcontrol(1, "d2d_module_finalize");
#endif
  module_finalize();
#ifdef PROFILING
  MPI_Pcontrol(-1, "d2d_module_finalize");
#endif

  MPI_Finalize();
  return 0;
}
