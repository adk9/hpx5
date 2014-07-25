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

extern int nsources; ///< number of source points
extern int ntargets; ///< number of target points
extern int datatype; ///< type of data to generate
extern int accuracy; ///< accuracy of the computation
extern int s; ///< partition criterion on the box

extern hpx_addr_t sources; ///< locations of the sources
extern hpx_addr_t charges; ///< strengths of the sources
extern hpx_addr_t targets; ///< locations of the targets
extern hpx_addr_t potential; ///< potential at the target locations
extern hpx_addr_t field; ///< field at the target locations

extern hpx_addr_t source_root; ///< pointer to the root of the source tree
extern hpx_addr_t target_root; ///< pointer to the root of the target tree

extern hpx_addr_t mapsrc; ///< source mapping info
extern hpx_addr_t maptar; ///< target mapping info

#endif
