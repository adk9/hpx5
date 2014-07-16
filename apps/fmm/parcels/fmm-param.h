/// ----------------------------------------------------------------------------
/// @file fmm-param.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief Declare functions that compute various translation coeffs
/// ----------------------------------------------------------------------------
#pragma once
#ifndef FMM_PARAM_H 
#define FMM_PARAM_H
#include "fmm-types.h"

#define MAXLEVEL 128

/// ----------------------------------------------------------------------------
/// @brief The parameter itself
/// ----------------------------------------------------------------------------
extern fmm_param_t *fmm_param; 

/// ----------------------------------------------------------------------------
/// @brief Destructs the parameter
/// ----------------------------------------------------------------------------
void destruct_param(fmm_param_t *fmm_param); 

/// ----------------------------------------------------------------------------
/// @brief Computes the binomial coefficents binom(p, n), where n = 0, ..., p
/// ----------------------------------------------------------------------------
void bnlcft(double *c, int p);

/// ----------------------------------------------------------------------------
/// @brief Returns a set of Gaussian nodes and weights for integrating the 
///        functions j0(r * x) * exp(z * x) dx over the range x = 0 to 
///        x = infinity
/// ----------------------------------------------------------------------------
void vwts(fmm_param_t *fmm_param);

/// ----------------------------------------------------------------------------
/// @brief Computes number of Fourier modes needed 
/// ----------------------------------------------------------------------------
void numthetahalf(fmm_param_t *fmm_param);

/// ----------------------------------------------------------------------------
/// @brief Creates factorial scaling factors
/// ----------------------------------------------------------------------------
void frmini(fmm_param_t *fmm_param);

/// ----------------------------------------------------------------------------
/// @brief Precomputes the rotation matrix
/// ----------------------------------------------------------------------------
void rotgen(fmm_param_t *fmm_param);

/// ----------------------------------------------------------------------------
/// @brief Implement the fast version of rotation matrices from recursion
/// ----------------------------------------------------------------------------
void fstrtn(int p, double *d, const double *sqc, double theta, int pgsz);

/// ----------------------------------------------------------------------------
/// @brief Computes number of Fourier modes needed 
/// ----------------------------------------------------------------------------
void numthetafour(fmm_param_t *fmm_param);

/// ----------------------------------------------------------------------------
/// @brief Precomputes coefficients needed by the TME operator
/// ----------------------------------------------------------------------------
void rlscini(fmm_param_t *fmm_param);

/// ----------------------------------------------------------------------------
/// @brief Precomputes exponentials needed in the TME->TEE->TEL operation
/// ----------------------------------------------------------------------------
void mkfexp(fmm_param_t *fmm_param);

/// ----------------------------------------------------------------------------
/// @brief Computes the tables of exponentials needed for translating 
///        exponential representations of harmonic functions
/// ----------------------------------------------------------------------------
void mkexps(fmm_param_t *fmm_param);

#endif
