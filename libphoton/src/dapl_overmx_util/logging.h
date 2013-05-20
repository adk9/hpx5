#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
extern int _photon_nproc, _photon_myrank;
#ifdef DEBUG
extern int _photon_start_debugging;
#endif

#if defined(DEBUG) || defined(CALLTRACE)
extern FILE *_grvl_ofp;
#define _photon_open_ofp() { if(_grvl_ofp == NULL){char name[10]; sprintf(name,"out.%05d",_photon_myrank);_grvl_ofp=fopen(name,"w"); } }
#endif

#ifdef CALLTRACE
#define ctr_info(fmt, args...)  do{ if(!_photon_start_debugging){break;} _photon_open_ofp(); fprintf(_grvl_ofp, "ALL:INF: %d: "fmt"\n", _photon_myrank, ##args); fflush(_grvl_ofp); }while(0);
#else
#define ctr_info(fmt, args...)  
#endif

#ifdef DEBUG
#define dbg_err(fmt, args...)   do{ if(!_photon_start_debugging){break;} fprintf(stderr, "ALL:ERR: %d: "fmt"\n", _photon_myrank, ##args); } while(0)
#define dbg_info(fmt, args...)  do{ if(!_photon_start_debugging){break;} _photon_open_ofp(); fprintf(_grvl_ofp, "ALL:INF: %d: "fmt"\n", _photon_myrank, ##args); fflush(_grvl_ofp); } while(0)

#define dbg_warn(fmt, args...)  do{ if(!_photon_start_debugging){break;} fprintf(stderr, "ALL:WRN: %d: "fmt"\n", _photon_myrank, ##args); } while(0)
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

#define log_err(fmt, args...)   fprintf(stderr, "ALL:ERR: %d: "fmt"\n", _photon_myrank, ##args)
#define log_info(fmt, args...)  fprintf(stderr, "ALL:INF: %d: "fmt"\n", _photon_myrank, ##args)
#define log_warn(fmt, args...)  fprintf(stderr, "ALL:WRN: %d: "fmt"\n", _photon_myrank, ##args)

#endif
