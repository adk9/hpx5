
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

MODULE DRIVER_BSPMA_MOD

CONTAINS

   SUBROUTINE DRIVER_BSPMA ( IERR )

   ! Purpose
   ! =======
   ! Driver for the bulk synchronous parallel with message aggregation 
   ! communication strategy. Called by the Fortran driver subroutine MINI_GHOST.
   
   USE MG_CONSTANTS_MOD
   USE MG_BSPMA_MOD
   USE MG_BSPMA_DIAGS_MOD
   USE MG_STENCIL_MOD
   USE MG_PROFILING_MOD
   USE MG_SUM_GRID_MOD

   IMPLICIT NONE

      INTEGER, INTENT(OUT) :: &
         IERR                       ! Return status

      ! ------------------
      ! Local Declarations
      ! ------------------
   
      INTEGER ::                  &
         IVAR,                    &  ! Counter (over variables)
         ISPIKE                      ! Counter (over NUM_SPIKES)
   
      REAL(KIND=MG_REAL) ::       &
         ERROR_ITER,              &  ! Difference between new and old GRIDi sum.
         GSUM                        ! Global SUM across GRIDs.

      REAL(KIND=MG_REAL8) ::      &
         TIME_START,              &
         TIME_START_2,            &
         TIME_START_ALL
   
      ! ---------------------
      ! Executable Statements
      ! ---------------------
   
      IERR = 0

#if defined _MG_OACC
!$acc data present_or_copyin ( &
!$acc               WORK,      &
!$acc               GRID1,  GRID2,  GRID3,  GRID4,  GRID5,  GRID6,  GRID7,  GRID8,  GRID9,  GRID10, &
!$acc               GRID11, GRID12, GRID13, GRID14, GRID15, GRID16, GRID17, GRID18, GRID19, GRID20, &
!$acc               GRID21, GRID22, GRID23, GRID24, GRID25, GRID26, GRID27, GRID28, GRID29, GRID30, &
!$acc               GRID31, GRID32, GRID33, GRID34, GRID35, GRID36, GRID37, GRID38, GRID39, GRID40 )
#if defined _MG_MPI
!$acc data present_or_copyin ( &
!$acc               SEND_BUFFER_BACK,  SEND_BUFFER_FRONT, &
!$acc               SEND_BUFFER_EAST,  SEND_BUFFER_WEST,  &
!$acc               SEND_BUFFER_NORTH, SEND_BUFFER_SOUTH, &
!$acc               RECV_BUFFER_BACK,  RECV_BUFFER_FRONT, &
!$acc               RECV_BUFFER_EAST,  RECV_BUFFER_WEST,  &
!$acc               RECV_BUFFER_NORTH, RECV_BUFFER_SOUTH )
#endif
#endif

      ! -------------------
      ! Begin time stepping
      ! -------------------

      TIME_START_ALL = MG_TIMER()

      DO ISPIKE = 1, NUM_SPIKES

         CALL MG_INSERT_SPIKE ( ISPIKE, IERR )

         DO TSTEP = 1, NUM_TSTEPS

#if defined _MG_MPI
            TIME_START = MG_TIMER()
            IF ( STENCIL == STENCIL_2D5PT .OR. STENCIL == STENCIL_3D7PT ) THEN
               CALL MG_BSPMA ( IERR )
               CALL MG_ASSERT ( IERR, 'DRIVER_BSPMA: MG_BSPMA', TSTEP )
            ELSE IF ( STENCIL == STENCIL_2D9PT .OR. STENCIL == STENCIL_3D27PT ) THEN
               CALL MG_BSPMA_DIAGS ( IERR )
               CALL MG_ASSERT ( IERR, 'DRIVER_BSPMA: MG_BSPMA_DIAG', TSTEP )
            ELSE
               IERR = -1
               CALL MG_ASSERT ( IERR, 'DRIVER_BSPMA: Unknown stencil for boundary exchange', STENCIL )
            END IF
            MG_PERF%TIME_BSPMA_PE = MG_PERF%TIME_BSPMA_PE + MG_TIMER() - TIME_START
#endif

            IF ( STENCIL /= STENCIL_NONE ) THEN
               DO IVAR = 1, NUM_VARS
                  TIME_START = MG_TIMER()
                  CALL MG_STENCIL ( IVAR, IERR )
                  CALL MG_ASSERT ( IERR, 'DRIVER_BSPMA:MG_STENCIL', IVAR )
                  MG_PERF%TIME_STENCIL_PE = MG_PERF%TIME_STENCIL_PE + MG_TIMER() - TIME_START

                  ! Reduction across GRID option
                  IF ( GRIDS_TO_SUM(IVAR) ) THEN
                     TIME_START_2 = MG_TIMER ()
                     CALL MG_SUM_GRID ( IVAR, GSUM, IERR )
                     CALL MG_ASSERT ( IERR, 'DRIVER_BSPMA:MG_SUM_GRID', IVAR )
                     MG_PERF%TIME_SUMGRID_PE = MG_PERF%TIME_SUMGRID_PE + MG_TIMER () - TIME_START_2

                     IF ( MYPE == ROOT_PE ) THEN
                        ERROR_ITER = ABS ( SOURCE_TOTAL(IVAR) - GSUM ) / SOURCE_TOTAL(IVAR)
                        IF ( ERROR_ITER > ERROR_TOL ) THEN
                           WRITE(*,99) TSTEP, IVAR, ERROR_ITER, ERROR_TOL
                           IERR = -1
                           CALL MG_ASSERT ( IERR, 'DRIVER_BSPMA', IVAR )
                        END IF
                        IF ( MOD ( TSTEP, REPORT_DIFFUSION ) == 0 ) THEN
                           WRITE(*,99) TSTEP, IVAR, ERROR_ITER, ERROR_TOL
                        END IF
                     END IF

                  END IF
               END DO
            END IF
         END DO ! Time step
      END DO    ! SPIKE insertion.

#if defined _MG_OACC
!$acc end data
!$acc end data
#endif

      MG_PERF%TIME_WALL_PE = MG_TIMER() - TIME_START_ALL
   
      RETURN
 
 99   FORMAT ( 'Time step ', I4, ' for variable ', I4, ' the error is ', 1PE12.5 ' ; error tolerance is ', 1PE8.2, '.' )

   END SUBROUTINE DRIVER_BSPMA

END MODULE DRIVER_BSPMA_MOD
