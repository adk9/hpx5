/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Platform-specific "count leading zeros."
  
  clz.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
  Luke Dalessandro <ldalessa [at] indiana.edu>
  ====================================================================
*/

#ifndef HPX_CLZ_H_
#define HPX_CLZ_H_

/**
 * @brief Deal with the different builtin interfaces that compilers provide for
 *        the "count leading zeros" operation we use in quickly computing
 *        floor(log_2(int)).
 **/
#if defined(__GNUC__)
static inline unsigned long clz(iunsigned long l) {
  return __builtin_clzl(l);
}
#else
unsigned long clz(iunsigned long l);
#endif

#endif /* HPX_CLZ_H_ */
