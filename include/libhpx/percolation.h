// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifndef LIBHPX_PERCOLATION_H
#define LIBHPX_PERCOLATION_H

/// @file include/libhpx/percolation.h
/// @brief Exports methods and structures related to percolation.
///

#ifdef __cplusplus
extern "C" {
#endif

#include "hpx/attributes.h"
#include "hpx/hpx.h"
#include "libhpx/config.h"

typedef struct percolation {
  const char *(*id)(void);
  void (*delete)(struct percolation*);
  void *(*prepare)(const struct percolation*, const char *, const char *);
  int (*execute)(const struct percolation*, void *, int, void **, size_t *);
  void (*destroy)(const struct percolation*, void *);
} percolation_t;

percolation_t *percolation_new_opencl(void);
percolation_t *percolation_new(void);

/// Percolation  interface.
/// @{

/// Delete a percolation object.
static inline void percolation_delete(percolation_t *percolation) {
  percolation->delete(percolation);
}

/// Percolate an action to be executed later on a device.
static inline
void *percolation_prepare(const percolation_t *percolation,
                          const char *key, const char *kernel) {
  return percolation->prepare(percolation, key, kernel);
}

/// Execute a percolation object on an available device.
static inline
int percolation_execute(const percolation_t *percolation, void *obj,
                        int nargs, void *vargs[], size_t sizes[]) {
  return percolation->execute(percolation, obj, nargs, vargs, sizes);
}

/// Destroy or release the state associated with a percolation object.
static inline void percolation_destroy(const percolation_t *percolation, void *obj) {
  percolation->destroy(percolation, obj);
}
   
/// An action to launch OpenCL kernels.
extern HPX_ACTION_DECL(percolation_execute_action);

/// The action handler that launches OpenCL kernels.
int percolation_execute_handler(int nargs, void *vargs[],
        size_t sizes[]);
/// @}


#ifdef __cplusplus
}
#endif

#endif // LIBHPX_PERCOLATION_H
