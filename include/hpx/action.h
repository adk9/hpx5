/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

/* An HPX action taking a single generic (void*) argument */
typedef struct hpx_action_t {
  char *name;          /* name of the action */
  hpx_func_t *action;  /* handle to the function */
} hpx_action_t;

/* Register a new action */
int hpx_action_register(char *, hpx_func_t);

/* Lookup actions in the local action table by their name */
int hpx_action_lookup_local(char *, hpx_func_t *);

/* Reverse lookup of actions in the local action table */
int hpx_action_lookup_byaddr_local(void *, hpx_func_t *);
