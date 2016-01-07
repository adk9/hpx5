dnl 
dnl  Copyright (c) 2006 The Trustees of Indiana University and Indiana
dnl                     University Research and Technology
dnl                     Corporation.  All rights reserved.
dnl  Copyright (c) 2006 The Technical University of Chemnitz. All 
dnl                     rights reserved.
dnl 
dnl  Author(s): Torsten Hoefler <htor@cs.indiana.edu>
dnl 
AC_DEFUN([NBC_CHECK_MPI],
  # don't require mpicc if we are inside OMPI (this variable is set
  # there)
  if test x"${HAVE_OMPI}" = "x"; then
    [AC_ARG_WITH(mpi,
     AC_HELP_STRING([--with-mpi], [compile with MPI support (ARG can be the path to the root MPI directory, if mpicc is not in PATH)]),
    )
      if test x${MPICC} == x; then
          MPICC=mpicc
      fi;
      if test x"${withval-yes}" != xyes; then
        AC_CHECK_PROG(mpicc_found, $MPICC, yes, no, ${withval}/bin)
        mpicc_path=${withval}/bin/
      else
        AC_CHECK_PROG(mpicc_found, $MPICC, yes, no)
        mpicc_path=
      fi
      if test "x${mpicc_found}" = "xno"; then
        AC_MSG_ERROR(${mpicc_path}mpicc not found)
      else
        CC=${mpicc_path}$MPICC
        AC_DEFINE(HAVE_MPI, 1, enables MPI code)
      fi
      unset mpicc_path mpicc_found
     ]
  fi
)
