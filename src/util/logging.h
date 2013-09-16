#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <mpi.h>
#include <pthread.h>
extern int _photon_nproc, _photon_myrank;
#ifdef DEBUG
extern int _photon_start_debugging;
#endif

#if defined(DEBUG) || defined(CALLTRACE)
extern FILE *_phot_ofp;
#define _photon_open_ofp() { if(_phot_ofp == NULL){char name[10]; int global_rank; MPI_Comm_rank(MPI_COMM_WORLD, &global_rank); sprintf(name,"out.%05d",global_rank);_phot_ofp=fopen(name,"w"); } }
#endif

#ifdef DEBUG
#define dbg_err(fmt, args...)   do{ if(!_photon_start_debugging){break;} fprintf(stderr, "ALL:ERR: %d (%d): > %s(): "fmt"\n", _photon_myrank, (int)pthread_self(), __FUNCTION__, ##args); } while(0)
#define dbg_info(fmt, args...)  do{ if(!_photon_start_debugging){break;} _photon_open_ofp(); fprintf(_phot_ofp, "ALL:INF: %d (%d): > %s(): "fmt"\n", _photon_myrank, (int)pthread_self(), __FUNCTION__, ##args); fflush(_phot_ofp); } while(0)

#define dbg_warn(fmt, args...)  do{ if(!_photon_start_debugging){break;} fprintf(stderr, "ALL:WRN: %d (%d): > %s(): "fmt"\n", _photon_myrank, (int)pthread_self(), __FUNCTION__, ##args); } while(0)
#else
#define dbg_err(fmt, args...)
#define dbg_info(fmt, args...)
#define dbg_warn(fmt, args...)
#endif

#define one_info(fmt, args...)	do{ if (_photon_myrank == 0) { fprintf(stderr, "ONE:INF:"fmt"\n", ##args); } } while (0)
#define one_stat(fmt, args...)	do{ if (_photon_myrank == 0) { fprintf(stderr, "ONE:STT:"fmt"\n", ##args); } } while (0)
#define one_warn(fmt, args...)	do{ if (_photon_myrank == 0) { fprintf(stderr, "ONE:WRN:"fmt"\n", ##args); } } while (0)
#define one_err(fmt, args...)	do{ if (_photon_myrank == 0) { fprintf(stderr, "ONE:ERR:"fmt"\n", ##args); } } while (0)

#ifdef DEBUG
#define one_debug(fmt, args...)	do{ if (_photon_myrank == 0) { fprintf(stderr, "ONE:DBG:"fmt"\n", ##args); } } while (0)
#else
#define one_debug(fmt, args...)
#endif

#define log_err(fmt, args...)   fprintf(stderr, "ALL:ERR: %d: > %s(): "fmt"\n", _photon_myrank, __FUNCTION__, ##args)
#define log_info(fmt, args...)  fprintf(stderr, "ALL:INF: %d: > %s(): "fmt"\n", _photon_myrank, __FUNCTION__, ##args)
#define log_warn(fmt, args...)  fprintf(stderr, "ALL:WRN: %d: > %s(): "fmt"\n", _photon_myrank, __FUNCTION__, ##args)
#define init_err(fmt, args...)  fprintf(stderr, "ALL:ERR: %d: > %s(): Library not initialized.  Call photon_init() first "fmt"\n", _photon_myrank, __FUNCTION__, ##args);

#endif
