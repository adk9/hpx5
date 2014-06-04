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

MODULE MG_GET_FACE_MOD

   ! Purpose
   ! =======
   ! Unpack received data buffers into variable faces.

   USE MG_CONSTANTS_MOD
   USE MG_UTILS_MOD

   IMPLICIT NONE

CONTAINS

   SUBROUTINE GET_FACE ( WHICH, OFFSET, GRID, IERR )

      ! ---------------------
      ! Argument Declarations
      ! ---------------------

      INTEGER, INTENT(IN) :: &
         WHICH

      INTEGER, INTENT(INOUT) :: &
         OFFSET

      INTEGER, INTENT(OUT) :: &
         IERR

      REAL(KIND=MG_REAL), DIMENSION(0:NX+1, 0:NY+1, 0:NZ+1), INTENT(OUT) :: &
         GRID

      ! ------------------
      ! Local Declarations
      ! ------------------

      INTEGER :: &
         I, J, K                  ! Counters

      ! ---------------------
      ! Executable Statements
      ! ---------------------

      IERR = 0

      SELECT CASE ( WHICH )

         CASE ( BACK )

!$OMP PARALLEL DO
            DO J = 0, NY+1
               DO I = 0, NX+1
                  GRID ( I, J, 0 ) = RECV_BUFFER_BACK(OFFSET + J*(NX+2) + I+1)
               END DO
            END DO
!$OMP END PARALLEL DO
            OFFSET = OFFSET + (NY+2)*(NX+2)

         CASE ( FRONT )

!$OMP PARALLEL DO
            DO J = 0, NY+1
               DO I = 0, NX+1
                  GRID ( I, J, NZ+1 ) = RECV_BUFFER_FRONT(OFFSET + J*(NX+2) + I+1)
               END DO
            END DO
!$OMP END PARALLEL DO
            OFFSET = OFFSET + (NY+2)*(NX+2)

         CASE ( EAST )

!$OMP PARALLEL DO
            DO K = 0, NZ+1
               DO J = 0, NY+1
                  GRID ( NX+1, J, K ) = RECV_BUFFER_EAST(OFFSET + K*(NY+2) + J+1)
               END DO
            END DO
!$OMP END PARALLEL DO
            OFFSET = OFFSET + (NZ+2)*(NY+2)

         CASE ( WEST )

!$OMP PARALLEL DO
            DO K = 0, NZ+1
               DO J = 0, NY+1
                  GRID ( 0, J, K ) = RECV_BUFFER_WEST(OFFSET + K*(NY+2) + J+1)
               END DO
            END DO
!$OMP END PARALLEL DO
            OFFSET = OFFSET + (NZ+2)*(NY+2)

         CASE ( NORTH )

!$OMP PARALLEL DO
            DO K = 0, NZ+1
               DO I = 0, NX+1
                  GRID ( I, NY+1, K ) = RECV_BUFFER_NORTH(OFFSET + K*(NX+2) + I+1)
               END DO
            END DO
!$OMP END PARALLEL DO
            OFFSET = OFFSET + (NZ+2)*(NX+2)

         CASE ( SOUTH )

!$OMP PARALLEL DO
            DO K = 0, NZ+1
               DO I = 0, NX+1
                  GRID ( I, 0, K ) = RECV_BUFFER_SOUTH(OFFSET + K*(NX+2) + I+1)
               END DO
            END DO
!$OMP END PARALLEL DO
            OFFSET = OFFSET + (NZ+2)*(NX+2)

      END SELECT

   END SUBROUTINE GET_FACE

   SUBROUTINE MG_GET_FACE ( IVAR, WHICH, OFFSET, IERR )
   
      ! ---------------------
      ! Argument Declarations
      ! ---------------------

      INTEGER(KIND=MG_INT), INTENT(IN) :: &
         IVAR,                             &  ! Which variable.
         WHICH                                ! Which boundary (north, south, etc)

      INTEGER(KIND=MG_INT), INTENT(INOUT) :: &
         OFFSET

      INTEGER(KIND=MG_INT), INTENT(OUT) :: &
         IERR           ! Return status

      ! ------------------
      ! Local Declarations
      ! ------------------
   
      INTEGER(KIND=MG_INT) :: &
         I, J                            ! Counters
   
      ! ---------------------
      ! Executable Statements
      ! ---------------------

      IERR = 0

      SELECT CASE ( IVAR )

         CASE ( 1 )

            CALL GET_FACE ( WHICH, OFFSET, GRID1, IERR )

         CASE ( 2 )

            CALL GET_FACE ( WHICH, OFFSET, GRID2, IERR )

         CASE ( 3 )

            CALL GET_FACE ( WHICH, OFFSET, GRID3, IERR )

         CASE ( 4 )

            CALL GET_FACE ( WHICH, OFFSET, GRID4, IERR )

         CASE ( 5 )

            CALL GET_FACE ( WHICH, OFFSET, GRID5, IERR )

         CASE ( 6 )

            CALL GET_FACE ( WHICH, OFFSET, GRID6, IERR )

         CASE ( 7 )

            CALL GET_FACE ( WHICH, OFFSET, GRID7, IERR )

         CASE ( 8 )

            CALL GET_FACE ( WHICH, OFFSET, GRID8, IERR )

         CASE ( 9 )

            CALL GET_FACE ( WHICH, OFFSET, GRID9, IERR )

         CASE ( 10 )

            CALL GET_FACE ( WHICH, OFFSET, GRID10, IERR )

         CASE ( 11 )

            CALL GET_FACE ( WHICH, OFFSET, GRID11, IERR )

         CASE ( 12 )

            CALL GET_FACE ( WHICH, OFFSET, GRID12, IERR )

         CASE ( 13 )

            CALL GET_FACE ( WHICH, OFFSET, GRID13, IERR )

         CASE ( 14 )

            CALL GET_FACE ( WHICH, OFFSET, GRID14, IERR )

         CASE ( 15 )

            CALL GET_FACE ( WHICH, OFFSET, GRID15, IERR )

         CASE ( 16 )

            CALL GET_FACE ( WHICH, OFFSET, GRID16, IERR )

         CASE ( 17 )

            CALL GET_FACE ( WHICH, OFFSET, GRID17, IERR )

         CASE ( 18 )

            CALL GET_FACE ( WHICH, OFFSET, GRID18, IERR )

         CASE ( 19 )

            CALL GET_FACE ( WHICH, OFFSET, GRID19, IERR )

         CASE ( 20 )

            CALL GET_FACE ( WHICH, OFFSET, GRID20, IERR )

         CASE ( 21 )

            CALL GET_FACE ( WHICH, OFFSET, GRID21, IERR )

         CASE ( 22 )

            CALL GET_FACE ( WHICH, OFFSET, GRID22, IERR )

         CASE ( 23 )

            CALL GET_FACE ( WHICH, OFFSET, GRID23, IERR )

         CASE ( 24 )

            CALL GET_FACE ( WHICH, OFFSET, GRID24, IERR )

         CASE ( 25 )

            CALL GET_FACE ( WHICH, OFFSET, GRID25, IERR )

         CASE ( 26 )

            CALL GET_FACE ( WHICH, OFFSET, GRID26, IERR )

         CASE ( 27 )

            CALL GET_FACE ( WHICH, OFFSET, GRID27, IERR )

         CASE ( 28 )

            CALL GET_FACE ( WHICH, OFFSET, GRID28, IERR )

         CASE ( 29 )

            CALL GET_FACE ( WHICH, OFFSET, GRID29, IERR )

         CASE ( 30 )

            CALL GET_FACE ( WHICH, OFFSET, GRID30, IERR )

         CASE ( 31 )

            CALL GET_FACE ( WHICH, OFFSET, GRID31, IERR )

         CASE ( 32 )

            CALL GET_FACE ( WHICH, OFFSET, GRID32, IERR )

         CASE ( 33 )

            CALL GET_FACE ( WHICH, OFFSET, GRID33, IERR )

         CASE ( 34 )

            CALL GET_FACE ( WHICH, OFFSET, GRID34, IERR )

         CASE ( 35 )

            CALL GET_FACE ( WHICH, OFFSET, GRID35, IERR )

         CASE ( 36 )

            CALL GET_FACE ( WHICH, OFFSET, GRID36, IERR )

         CASE ( 37 )

            CALL GET_FACE ( WHICH, OFFSET, GRID37, IERR )

         CASE ( 38 )

            CALL GET_FACE ( WHICH, OFFSET, GRID38, IERR )

         CASE ( 39 )

            CALL GET_FACE ( WHICH, OFFSET, GRID39, IERR )

         CASE ( 40 )

            CALL GET_FACE ( WHICH, OFFSET, GRID40, IERR )

      END SELECT

      RETURN
   
   END SUBROUTINE MG_GET_FACE

END MODULE MG_GET_FACE_MOD
