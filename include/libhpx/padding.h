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
#ifndef LIBHPX_PADDING_H
#define LIBHPX_PADDING_H

/// Given a number of bytes, how many bytes of padding do we need to get a size
/// that is a multiple of HPX_CACHELINE_SIZE? Macro because it's used in
/// structure definitions for padding.

#define _CAT1(S, T) S##T
#define _CAT(S, T) _CAT1(S, T)
#define _BYTES(S) (HPX_CACHELINE_SIZE - ((S) % HPX_CACHELINE_SIZE))
#define PAD_TO_CACHELINE(S) const char _CAT(_padding,__LINE__)[_BYTES(S)]

#endif // LIBHPX_PADDING_H
