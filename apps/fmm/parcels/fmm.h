#pragma once
#ifndef FMM_H
#define FMM_H

#include "fmm-types.h"
#include "fmm-action.h"
#include "fmm-param.h"
#include "fmm-dag.h"
#include "hpx/hpx.h"

fmm_param_t fmm_param; 
hpx_addr_t mpole; 
hpx_addr_t local_h;
hpx_addr_t local_v; 
hpx_addr_t expu; 
hpx_addr_t expd; 
hpx_addr_t expn;
hpx_addr_t exps;
hpx_addr_t expe; 
hpx_addr_t expw; 
#endif
