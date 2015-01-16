#ifndef D2DMODULE_H_
#define D2DMODULE_H_

#include <stdint.h>
#include <stdio.h>
#include <mpi.h>

extern int nx;    /* number of discrete points including endpoints */
extern int ny;    /* number of rows of points, including endpoints */
extern double k;  /* D*dt/(dx*dx) */
extern int nsteps;/* number of time steps */
extern int wstep; /* write frame every this many time steps */
extern int nprocsx;    /* number of procs in the x direction */
extern int nprocsy;    /* number of procs in the y direction */
extern int nxl;        /* horizontal extent of one process; divides nprocsx */
extern int nyl;        /* vertical extent of one processl; divides nprocsy */
extern int nprocs;     /* number of processes */
extern int rank;       /* the rank of this process */
extern int xcoord;     /* the horizontal coordinate of this proc */
extern int ycoord;     /* the vertical coordinate of this proc */
extern int up;         /* rank of upper neighbor on torus */
extern int down;       /* rank of lower neighbor on torus */
extern int left;       /* rank of left neighbor on torus */
extern int right;      /* rank of right neighbor on torus */
extern double **u;     /* [nxl+2][nyl+2]; values of the discretized function */

extern char fileuri[256];
extern MPI_Datatype filetype;
extern MPI_File filehandle;
extern int phorwarder;
extern uint32_t photon_request;
extern double *iobuf;
extern int iobuf_size;
extern FILE *outfile;

void module_init();
void pre_exchange(int io_frame);
void write_frame(int time);
void module_finalize();

#endif /* D2DMODULE_H_ */
