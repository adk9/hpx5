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

MODULE MG_STENCIL_MOD

   ! Purpose
   ! =======
   ! Selection of the appropriate stencil computation on the input variable.

   USE MG_UTILS_MOD
   USE MG_STENCIL_COMPS_MOD

   IMPLICIT NONE

   ! Note: Using 0-based indexing on arrays.

CONTAINS
   
   SUBROUTINE MG_STENCIL ( IVAR, IERR )

      ! Options:
      ! ---------------
      ! 1) 3d 27pt

      ! ---------------------
      ! Argument Declarations
      ! ---------------------

      INTEGER, INTENT(IN) :: &
         IVAR           ! Variable to be operated on.

      INTEGER, INTENT(OUT) :: &
         IERR           ! Return status

      ! ------------------
      ! Local Declarations
      ! ------------------
   
      ! ---------------------
      ! Executable Statements
      ! ---------------------
   
      IERR = 0

      SELECT CASE ( STENCIL )  

         CASE ( STENCIL_3D7PT )

            SELECT CASE ( IVAR )

               CASE ( 1 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID1, IERR )
               CASE ( 2 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID2, IERR )
               CASE ( 3 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID3, IERR )
               CASE ( 4 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID4, IERR )
               CASE ( 5 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID5, IERR )
               CASE ( 6 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID6, IERR )
               CASE ( 7 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID7, IERR )
               CASE ( 8 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID8, IERR )
               CASE ( 9 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID9, IERR )
               CASE ( 10 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID10, IERR )
               CASE ( 11 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID11, IERR )
               CASE ( 12 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID12, IERR )
               CASE ( 13 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID13, IERR )
               CASE ( 14 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID14, IERR )
               CASE ( 15 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID15, IERR )
               CASE ( 16 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID16, IERR )
               CASE ( 17 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID17, IERR )
               CASE ( 18 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID18, IERR )
               CASE ( 19 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID19, IERR )
               CASE ( 20 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID20, IERR )
               CASE ( 21 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID21, IERR )
               CASE ( 22 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID22, IERR )
               CASE ( 23 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID23, IERR )
               CASE ( 24 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID24, IERR )
               CASE ( 25 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID25, IERR )
               CASE ( 26 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID26, IERR )
               CASE ( 27 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID27, IERR )
               CASE ( 28 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID28, IERR )
               CASE ( 29 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID29, IERR )
               CASE ( 30 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID30, IERR )
               CASE ( 31 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID31, IERR )
               CASE ( 32 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID32, IERR )
               CASE ( 33 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID33, IERR )
               CASE ( 34 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID34, IERR )
               CASE ( 35 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID35, IERR )
               CASE ( 36 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID36, IERR )
               CASE ( 37 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID37, IERR )
               CASE ( 38 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID38, IERR )
               CASE ( 39 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID39, IERR )
               CASE ( 40 )
                  CALL MG_STENCIL_3D7PT ( IVAR, GRID40, IERR )
            END SELECT

         CASE ( STENCIL_3D27PT )

            SELECT CASE ( IVAR )

               CASE ( 1 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID1, IERR )
               CASE ( 2 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID2, IERR )
               CASE ( 3 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID3, IERR )
               CASE ( 4 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID4, IERR )
               CASE ( 5 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID5, IERR )
               CASE ( 6 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID6, IERR )
               CASE ( 7 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID7, IERR )
               CASE ( 8 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID8, IERR )
               CASE ( 9 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID9, IERR )
               CASE ( 10 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID10, IERR )
               CASE ( 11 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID11, IERR )
               CASE ( 12 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID12, IERR )
               CASE ( 13 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID13, IERR )
               CASE ( 14 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID14, IERR )
               CASE ( 15 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID15, IERR )
               CASE ( 16 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID16, IERR )
               CASE ( 17 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID17, IERR )
               CASE ( 18 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID18, IERR )
               CASE ( 19 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID19, IERR )
               CASE ( 20 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID20, IERR )
               CASE ( 21 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID21, IERR )
               CASE ( 22 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID22, IERR )
               CASE ( 23 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID23, IERR )
               CASE ( 24 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID24, IERR )
               CASE ( 25 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID25, IERR )
               CASE ( 26 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID26, IERR )
               CASE ( 27 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID27, IERR )
               CASE ( 28 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID28, IERR )
               CASE ( 29 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID29, IERR )
               CASE ( 30 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID30, IERR )
               CASE ( 31 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID31, IERR )
               CASE ( 32 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID32, IERR )
               CASE ( 33 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID33, IERR )
               CASE ( 34 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID34, IERR )
               CASE ( 35 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID35, IERR )
               CASE ( 36 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID36, IERR )
               CASE ( 37 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID37, IERR )
               CASE ( 38 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID38, IERR )
               CASE ( 39 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID39, IERR )
               CASE ( 40 )
                  CALL MG_STENCIL_3D27PT ( IVAR, GRID40, IERR )
            END SELECT

         CASE ( STENCIL_2D5PT )

            SELECT CASE ( IVAR )

               CASE ( 1 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID1, IERR )
               CASE ( 2 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID2, IERR )
               CASE ( 3 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID3, IERR )
               CASE ( 4 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID4, IERR )
               CASE ( 5 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID5, IERR )
               CASE ( 6 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID6, IERR )
               CASE ( 7 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID7, IERR )
               CASE ( 8 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID8, IERR )
               CASE ( 9 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID9, IERR )
               CASE ( 10 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID10, IERR )
               CASE ( 11 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID11, IERR )
               CASE ( 12 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID12, IERR )
               CASE ( 13 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID13, IERR )
               CASE ( 14 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID14, IERR )
               CASE ( 15 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID15, IERR )
               CASE ( 16 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID16, IERR )
               CASE ( 17 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID17, IERR )
               CASE ( 18 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID18, IERR )
               CASE ( 19 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID19, IERR )
               CASE ( 20 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID20, IERR )
               CASE ( 21 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID21, IERR )
               CASE ( 22 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID22, IERR )
               CASE ( 23 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID23, IERR )
               CASE ( 24 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID24, IERR )
               CASE ( 25 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID25, IERR )
               CASE ( 26 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID26, IERR )
               CASE ( 27 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID27, IERR )
               CASE ( 28 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID28, IERR )
               CASE ( 29 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID29, IERR )
               CASE ( 30 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID30, IERR )
               CASE ( 31 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID31, IERR )
               CASE ( 32 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID32, IERR )
               CASE ( 33 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID33, IERR )
               CASE ( 34 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID34, IERR )
               CASE ( 35 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID35, IERR )
               CASE ( 36 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID36, IERR )
               CASE ( 37 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID37, IERR )
               CASE ( 38 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID38, IERR )
               CASE ( 39 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID39, IERR )
               CASE ( 40 )
                  CALL MG_STENCIL_2D5PT ( IVAR, GRID40, IERR )
            END SELECT

         CASE ( STENCIL_2D9PT )

            SELECT CASE ( IVAR )

               CASE ( 1 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID1, IERR )
               CASE ( 2 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID2, IERR )
               CASE ( 3 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID3, IERR )
               CASE ( 4 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID4, IERR )
               CASE ( 5 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID5, IERR )
               CASE ( 6 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID6, IERR )
               CASE ( 7 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID7, IERR )
               CASE ( 8 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID8, IERR )
               CASE ( 9 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID9, IERR )
               CASE ( 10 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID10, IERR )
               CASE ( 11 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID11, IERR )
               CASE ( 12 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID12, IERR )
               CASE ( 13 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID13, IERR )
               CASE ( 14 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID14, IERR )
               CASE ( 15 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID15, IERR )
               CASE ( 16 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID16, IERR )
               CASE ( 17 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID17, IERR )
               CASE ( 18 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID18, IERR )
               CASE ( 19 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID19, IERR )
               CASE ( 20 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID20, IERR )
               CASE ( 21 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID21, IERR )
               CASE ( 22 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID22, IERR )
               CASE ( 23 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID23, IERR )
               CASE ( 24 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID24, IERR )
               CASE ( 25 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID25, IERR )
               CASE ( 26 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID26, IERR )
               CASE ( 27 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID27, IERR )
               CASE ( 28 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID28, IERR )
               CASE ( 29 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID29, IERR )
               CASE ( 30 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID30, IERR )
               CASE ( 31 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID31, IERR )
               CASE ( 32 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID32, IERR )
               CASE ( 33 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID33, IERR )
               CASE ( 34 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID34, IERR )
               CASE ( 35 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID35, IERR )
               CASE ( 36 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID36, IERR )
               CASE ( 37 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID37, IERR )
               CASE ( 38 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID38, IERR )
               CASE ( 39 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID39, IERR )
               CASE ( 40 )
                  CALL MG_STENCIL_2D9PT ( IVAR, GRID40, IERR )
            END SELECT

         CASE DEFAULT

            WRITE(*,*) ' ** Error ** MG_STENCIL: Unknown STENCIL = ', STENCIL
            IERR = -1

      END SELECT

   END SUBROUTINE MG_STENCIL
   
END MODULE MG_STENCIL_MOD
