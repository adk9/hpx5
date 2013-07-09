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

typedef struct hpx_parcel_t {
    unsigned int  parcel_id;   /* the parcel idenitifer */
    char         *aname;       /* name of the associated action */
    hpx_action_t  action;      /* handle to the associated action */
    hpx_action_t  caction;     /* handle to the continuation action */
    int           flags;       /* flags related to the parcel */
} hpx_parcel_t;


/*
 --------------------------------------------------------------------
  Parcel Registration
  -------------------------------------------------------------------
*/
int hpx_parcel_register(char *, hpx_action_t, hpx_parcel_t *);
