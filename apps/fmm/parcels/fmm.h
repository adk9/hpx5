/// ----------------------------------------------------------------------------
/// @file fmm.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief FMM header file
/// ----------------------------------------------------------------------------
#pragma once
#ifndef FMM_H
#define FMM_H

#include "hpx/hpx.h"
#include "fmm-types.h"
#include "fmm-param.h"
#include "fmm-action.h"

extern int nsources; ///< number of source points
extern int ntargets; ///< number of target points
extern int datatype; ///< type of data to generate
extern int accuracy; ///< accuracy of the computation
extern int s; ///< partition criterion on the box

#endif
