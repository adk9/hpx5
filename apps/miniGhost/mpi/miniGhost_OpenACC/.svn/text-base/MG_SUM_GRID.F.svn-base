! ************************************************************************
!
!               miniGhost: stencil computations with boundary exchange.
!                 Copyright (2012) Sandia Corporation
!
! Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
! license for use of this work by or on behalf of the U.S. Government.
!
! This library is free software; you can redistribute it and/or modify
! it under the terms of the GNU Lesser General Public License as
! published by the Free Software Foundation; either version 2.1 of the
! License, or (at your option) any later version.
!
! This library is distributed in the hope that it will be useful, but
! WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
! Lesser General Public License for more details.
!
! You should have received a copy of the GNU Lesser General Public
! License along with this library; if not, write to the Free Software
! Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
! USA
! Questions? Contact Richard F. Barrett (rfbarre@sandia.gov) or
!                    Michael A. Heroux (maherou@sandia.gov)
!
! ************************************************************************

MODULE MG_SUM_GRID_MOD

   ! Purpose
   ! =======
   ! Coordination of the grid summation on the appropriate variable.

   USE MG_UTILS_MOD
   USE MG_ALLREDUCE_MOD

   IMPLICIT NONE

CONTAINS

   SUBROUTINE MG_SUM_GRID ( IVAR, GSUM, IERR )

      ! ---------------------
      ! Argument Declarations
      ! ---------------------

      INTEGER, INTENT(IN) :: &
         IVAR           ! Variable to be reduced.

      REAL(KIND=MG_REAL), INTENT(OUT) :: & 
         GSUM

      INTEGER, INTENT(OUT) :: &
         IERR           ! Return status

      ! ------------------
      ! Local Declarations
      ! ------------------

      ! ---------------------
      ! Executable Statements
      ! ---------------------

      IERR = 0

      SELECT CASE( IVAR )

         CASE (1)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID1, GSUM, IERR )
         CASE (2)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID2, GSUM, IERR )
         CASE (3)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID3, GSUM, IERR )
         CASE (4)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID4, GSUM, IERR )
         CASE (5)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID5, GSUM, IERR )
         CASE (6)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID6, GSUM, IERR )
         CASE (7)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID7, GSUM, IERR )
         CASE (8)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID8, GSUM, IERR )
         CASE (9)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID9, GSUM, IERR )
         CASE (10)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID10, GSUM, IERR )
         CASE (11)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID11, GSUM, IERR )
         CASE (12)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID12, GSUM, IERR )
         CASE (13)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID13, GSUM, IERR )
         CASE (14)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID14, GSUM, IERR )
         CASE (15)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID15, GSUM, IERR )
         CASE (16)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID16, GSUM, IERR )
         CASE (17)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID17, GSUM, IERR )
         CASE (18)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID18, GSUM, IERR )
         CASE (19)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID19, GSUM, IERR )
         CASE (20)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID20, GSUM, IERR )
         CASE (21)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID21, GSUM, IERR )
         CASE (22)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID22, GSUM, IERR )
         CASE (23)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID23, GSUM, IERR )
         CASE (24)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID24, GSUM, IERR )
         CASE (25)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID25, GSUM, IERR )
         CASE (26)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID26, GSUM, IERR )
         CASE (27)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID27, GSUM, IERR )
         CASE (28)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID28, GSUM, IERR )
         CASE (29)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID29, GSUM, IERR )
         CASE (30)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID30, GSUM, IERR )
         CASE (31)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID31, GSUM, IERR )
         CASE (32)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID32, GSUM, IERR )
         CASE (33)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID33, GSUM, IERR )
         CASE (34)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID34, GSUM, IERR )
         CASE (35)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID35, GSUM, IERR )
         CASE (36)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID36, GSUM, IERR )
         CASE (37)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID37, GSUM, IERR )
         CASE (38)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID38, GSUM, IERR )
         CASE (39)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID39, GSUM, IERR )
         CASE (40)
            CALL MG_ALLREDUCE_SUM ( IVAR, GRID40, GSUM, IERR )
      END SELECT

      MG_PERF%NUM_SUMGRID = MG_PERF%NUM_SUMGRID + 1

   END SUBROUTINE MG_SUM_GRID

END MODULE MG_SUM_GRID_MOD
