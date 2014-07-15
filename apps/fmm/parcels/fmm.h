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
#include "fmm-dag.h"

extern hpx_addr_t sources; 
extern hpx_addr_t charges; 
extern hpx_addr_t targets; 
extern hpx_addr_t potential;
extern hpx_addr_t field; 
extern hpx_addr_t fmm_dag; 

#endif
