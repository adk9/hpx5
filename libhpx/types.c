// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <unistd.h>
#include <ffi.h>
#include "hpx/hpx.h"

#include "libhpx/config.h"
#include "libhpx/locality.h"
#include "libhpx/debug.h"


/// Register an HPX array datatype.
///
/// This is used to construct an array datatype that can be registered
/// as input for typed actions. The array consists of @p nelems
/// elements of the type @p basetype. The memory for the output array
/// type @p arrtype is allocated by this function; hence the caller
/// must call the corresponding `hpx_unregister_type` function to free
/// the allocated type.
int hpx_register_array_type(hpx_type_t *arrtype, hpx_type_t basetype, int nelems) {
  dbg_assert(arrtype);
  ffi_type *type = malloc(sizeof(ffi_type) + (nelems+1)*sizeof(ffi_type));
  if (!type) {
    return log_error("error allocating an HPX array datatype.\n");
  }

  type->size = 0;
  type->alignment = 0;
  type->type = FFI_TYPE_STRUCT;
  type->elements = (ffi_type**)((char*)type+(sizeof(ffi_type)));

  for (int i = 0; i < nelems; ++i) {
    type->elements[i] = basetype;
  }
  type->elements[nelems+1] = NULL;
  *arrtype = type;
  return HPX_SUCCESS;
}

/// Unregister an HPX datatype.
void hpx_unregister_type(hpx_type_t datatype) {
  if (datatype) {
    free(datatype);
  }
}
