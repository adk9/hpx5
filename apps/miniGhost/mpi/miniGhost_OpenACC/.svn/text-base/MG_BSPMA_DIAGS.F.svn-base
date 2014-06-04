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

MODULE MG_BSPMA_DIAGS_MOD

   ! BSPMA boundary exchange for 9- and 27-point stencils:    
   !    pack, send, receive, unpack.
   ! Coordinated exchange capturing diagonal elements in usual exchange.

   USE MG_CONSTANTS_MOD
   USE MG_UTILS_MOD
   USE MG_IRECV_MOD
   USE MG_PACK_MOD
   USE MG_SEND_BSPMA_MOD
   USE MG_UNPACK_BSPMA_MOD

   IMPLICIT NONE

CONTAINS
   
   SUBROUTINE MG_BSPMA_DIAGS ( IERR )
   
      ! ---------------------
      ! Argument Declarations
      ! ---------------------

      INTEGER, INTENT(OUT) :: &
         IERR           ! Return status

      ! ------------------
      ! Local Declarations
      ! ------------------

      INTEGER :: &
         I,           & ! Counter
         DIR(3)         ! Boundary exchange directions
   
      ! ---------------------
      ! Executable Statements
      ! ---------------------

      IERR = 0

      COUNT_SEND_NORTH = 0
      COUNT_SEND_SOUTH = 0
      COUNT_SEND_EAST  = 0
      COUNT_SEND_WEST  = 0
      COUNT_SEND_BACK  = 0
      COUNT_SEND_FRONT = 0

      DIR(1) = DIR_NORTH_SOUTH
      DIR(2) = DIR_EAST_WEST
      DIR(3) = DIR_BACK_FRONT

      DO I = 1, 3

         ! Set only those neighbors participating in this iterative loop.
         NEIGHBORS = -1
         SELECT CASE ( DIR(I) )
            CASE ( DIR_NORTH_SOUTH )
               NEIGHBORS(NORTH) = NEIGHBORS_ORIG(NORTH)
               NEIGHBORS(SOUTH) = NEIGHBORS_ORIG(SOUTH)
            CASE ( DIR_EAST_WEST )
               NEIGHBORS(EAST) = NEIGHBORS_ORIG(EAST)
               NEIGHBORS(WEST) = NEIGHBORS_ORIG(WEST)
            CASE ( DIR_BACK_FRONT )
               NEIGHBORS(BACK)  = NEIGHBORS_ORIG(BACK)
               NEIGHBORS(FRONT) = NEIGHBORS_ORIG(FRONT)
         END SELECT

         CALL MG_IRECV ( IERR )

         IF ( NUM_VARS > 0 ) &
            CALL MG_PACK ( GRID1,  IERR )
         IF ( NUM_VARS > 1 ) &
            CALL MG_PACK ( GRID2,  IERR )
         IF ( NUM_VARS > 2 ) &
            CALL MG_PACK ( GRID3,  IERR )
         IF ( NUM_VARS > 3 ) &
            CALL MG_PACK ( GRID4,  IERR )
         IF ( NUM_VARS > 4 ) &
            CALL MG_PACK ( GRID5,  IERR )
         IF ( NUM_VARS > 5 ) &
            CALL MG_PACK ( GRID6,  IERR )
         IF ( NUM_VARS > 6 ) &
            CALL MG_PACK ( GRID7,  IERR )
         IF ( NUM_VARS > 7 ) &
            CALL MG_PACK ( GRID8,  IERR )
         IF ( NUM_VARS > 8 ) &
            CALL MG_PACK ( GRID9,  IERR )
         IF ( NUM_VARS > 9 ) &
            CALL MG_PACK ( GRID10, IERR )
   
         IF ( NUM_VARS > 10 ) &
            CALL MG_PACK ( GRID11, IERR )
         IF ( NUM_VARS > 11 ) &
            CALL MG_PACK ( GRID12, IERR )
         IF ( NUM_VARS > 12 ) &
            CALL MG_PACK ( GRID13, IERR )
         IF ( NUM_VARS > 13 ) &
            CALL MG_PACK ( GRID14, IERR )
         IF ( NUM_VARS > 14 ) &
            CALL MG_PACK ( GRID15, IERR )
         IF ( NUM_VARS > 15 ) &
            CALL MG_PACK ( GRID16, IERR )
         IF ( NUM_VARS > 16 ) &
            CALL MG_PACK ( GRID17, IERR )
         IF ( NUM_VARS > 17 ) &
            CALL MG_PACK ( GRID18, IERR )
         IF ( NUM_VARS > 18 ) &
            CALL MG_PACK ( GRID19, IERR )
         IF ( NUM_VARS > 19 ) &
            CALL MG_PACK ( GRID20, IERR )
   
         IF ( NUM_VARS > 20 ) &
            CALL MG_PACK ( GRID21, IERR )
         IF ( NUM_VARS > 21 ) &
            CALL MG_PACK ( GRID22, IERR )
         IF ( NUM_VARS > 22 ) &
            CALL MG_PACK ( GRID23, IERR )
         IF ( NUM_VARS > 23 ) &
            CALL MG_PACK ( GRID24, IERR )
         IF ( NUM_VARS > 24 ) &
            CALL MG_PACK ( GRID25, IERR )
         IF ( NUM_VARS > 25 ) &
            CALL MG_PACK ( GRID26, IERR )
         IF ( NUM_VARS > 26 ) &
            CALL MG_PACK ( GRID27, IERR )
         IF ( NUM_VARS > 27 ) &
            CALL MG_PACK ( GRID28, IERR )
         IF ( NUM_VARS > 28 ) &
            CALL MG_PACK ( GRID29, IERR )
         IF ( NUM_VARS > 29 ) &
            CALL MG_PACK ( GRID30, IERR )
   
         IF ( NUM_VARS > 30 ) &
            CALL MG_PACK ( GRID31, IERR )
         IF ( NUM_VARS > 31 ) &
            CALL MG_PACK ( GRID32, IERR )
         IF ( NUM_VARS > 32 ) &
            CALL MG_PACK ( GRID33, IERR )
         IF ( NUM_VARS > 33 ) &
            CALL MG_PACK ( GRID34, IERR )
         IF ( NUM_VARS > 34 ) &
            CALL MG_PACK ( GRID35, IERR )
         IF ( NUM_VARS > 35 ) &
            CALL MG_PACK ( GRID36, IERR )
         IF ( NUM_VARS > 36 ) &
            CALL MG_PACK ( GRID37, IERR )
         IF ( NUM_VARS > 37 ) &
            CALL MG_PACK ( GRID38, IERR )
         IF ( NUM_VARS > 38 ) &
            CALL MG_PACK ( GRID39, IERR )
         IF ( NUM_VARS > 39 ) &
            CALL MG_PACK ( GRID40, IERR )
   
         CALL MG_SEND_BSPMA ( IERR )
   
         CALL MG_UNPACK_BSPMA ( IERR )

      END DO ! Loop over DIR

      NEIGHBORS(NORTH) = NEIGHBORS_ORIG(NORTH)
      NEIGHBORS(SOUTH) = NEIGHBORS_ORIG(SOUTH)
      NEIGHBORS(EAST)  = NEIGHBORS_ORIG(EAST)
      NEIGHBORS(WEST)  = NEIGHBORS_ORIG(WEST)
      NEIGHBORS(FRONT) = NEIGHBORS_ORIG(FRONT)
      NEIGHBORS(BACK)  = NEIGHBORS_ORIG(BACK)

   END SUBROUTINE MG_BSPMA_DIAGS
      
END MODULE MG_BSPMA_DIAGS_MOD
