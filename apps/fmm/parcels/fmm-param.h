#pragma once
#ifndef FMM_PARAM_H 
#define FMM_PARAM_H

#include "fmm-types.h"

fmm_param_t construct_param(int accuracy);

void destruct_param(fmm_param_t *fmm_param);

void frmini(fmm_param_t *fmm_param);

void rotgen(fmm_param_t *fmm_param);

void bnlcft(double *c, int p);

void fstrtn(int p, double *d, const double *sqc, double theta, int pgsz);

void vwts(fmm_param_t *fmm_param);

void numthetahalf(fmm_param_t *fmm_param);

void numthetafour(fmm_param_t *fmm_param);

void rlscini(fmm_param_t *fmm_param);

void mkfexp(fmm_param_t *fmm_param);

void mkexps(fmm_param_t *fmm_param);

#endif
