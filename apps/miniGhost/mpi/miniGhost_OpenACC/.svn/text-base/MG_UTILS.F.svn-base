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

MODULE MG_UTILS_MOD

   USE MG_CONSTANTS_MOD
   USE MG_OPTIONS_MOD

   IMPLICIT NONE

CONTAINS

   ! MG_INIT             : Set up MPI environment, problem input.
   ! MG_PRINT_HEADER     : Write configuration to STDIO.
   ! MG_GRID_INIT        : Allocate GRID arrays, call MG_INIT_GRID.
   ! MG_INSERT_SPIKE     : Insert heat sources.
   ! MG_GRID_DEALLOC     : Deallocate GRID arrays.
   ! MG_ASSERT           : Error checking.
   ! MG_INIT_GRID        : Initialize GRID arrays.
   ! MG_TIMER            : Returns time.
   ! MG_COMPUTE_STDDEV   : Compute standard deviation of input profiling data.

!  =================================================================================

   SUBROUTINE MG_INIT ( IERR )

      IMPLICIT NONE
   
      INTEGER ::  &
         IERR                    ! Return status.

      ! ---------------
      ! Local Variables
      ! ---------------

      INTEGER ::  &
         I,                    &    ! Counter.
         IX,                   &
         IY,                   &
         IZ,                   &
         J,                    &    ! Counter.
         MYPE_XY,              &    ! tmp var
         NVARS,                &    ! tmp var
         OFFSET_X,             &
         OFFSET_Y,             &
         OFFSET_Z,             &
         REMAINDER,            &
         XLOC,                 &
         YLOC,                 &
         ZLOC

      ! ------------
      ! Local Arrays
      ! ------------
 
      REAL(KIND=MG_REAL), DIMENSION(:,:), ALLOCATABLE ::   &
         RSPIKE_LOC            ! Temporary for random number generator.
   
      ! ---------------------
      ! Executable Statements 
      ! ---------------------

      IERR = 0
   
#if defined _MG_MPI
      ! Parallel processing configuration:

      CALL MPI_COMM_DUP ( MPI_COMM_WORLD, MPI_COMM_MG, IERR )

      CALL MPI_ERRHANDLER_SET ( MPI_COMM_MG, MPI_ERRORS_ARE_FATAL, IERR )

      CALL MPI_COMM_RANK ( MPI_COMM_MG, MYPE, IERR )

      CALL MPI_COMM_SIZE ( MPI_COMM_MG, NUMPES, IERR )
#elif defined _MG_SERIAL
      MYPE = 0    ! All formulas based on these values should work correctly.
      NUMPES = 1
#endif

      ! ---------------------------------
      ! Set position in 3d processor grid
      ! ---------------------------------

      MYPE_XY = MOD ( MYPE, NPX*NPY )
      MYPY = MYPE_XY / NPX
      REMAINDER = MOD ( MYPE_XY, NPX )
      IF ( REMAINDER /= 0 ) THEN
         MYPX = REMAINDER
      ELSE
         MYPX = 0
      END IF
      MYPZ = MYPE / ( NPX*NPY )

      ! --------------
      ! Set neighbors.
      ! --------------

      ALLOCATE ( NEIGHBORS(MAX_NUM_NEIGHBORS), STAT = IERR )
      NEIGHBORS(1:MAX_NUM_NEIGHBORS) = -1
      CALL MG_ASSERT ( IERR, 'MG_INIT: ALLOCATE ( NEIGHBORS )', MAX_NUM_NEIGHBORS )

      ALLOCATE ( NEIGHBORS_ORIG(MAX_NUM_NEIGHBORS), STAT = IERR )
      NEIGHBORS_ORIG(1:MAX_NUM_NEIGHBORS) = -1
      CALL MG_ASSERT ( IERR, 'MG_INIT: ALLOCATE ( NEIGHBORS_ORIG )', MAX_NUM_NEIGHBORS )

      NUM_NEIGHS = 0
      IF ( MYPY /= 0  ) THEN
         NEIGHBORS(SOUTH) = MYPE - NPX
         NUM_NEIGHS = NUM_NEIGHS + 1
      END IF
      IF ( MYPY /= NPY-1 ) THEN
         NEIGHBORS(NORTH) = MYPE + NPX
         NUM_NEIGHS = NUM_NEIGHS + 1
      END IF
      IF ( MYPX /= 0 ) THEN
         NEIGHBORS(WEST) = MYPE - 1
         NUM_NEIGHS = NUM_NEIGHS + 1
      END IF
      IF ( MYPX /= NPX-1 ) THEN
         NEIGHBORS(EAST) = MYPE + 1
         NUM_NEIGHS = NUM_NEIGHS + 1
      END IF
      IF ( MYPZ /= 0 ) THEN
         NEIGHBORS(BACK) = MYPE - ( NPX*NPY )
         NUM_NEIGHS = NUM_NEIGHS + 1
      END IF
      IF ( MYPZ /= NPZ-1 ) THEN
         NEIGHBORS(FRONT) = MYPE + ( NPX*NPY )
         NUM_NEIGHS = NUM_NEIGHS + 1
      END IF

      NEIGHBORS_ORIG = NEIGHBORS

      ALLOCATE ( SPIKES(NUM_VARS,NUM_SPIKES), STAT = IERR )
      CALL MG_ASSERT ( IERR, 'MG_INIT: ALLOCATE ( SPIKES )', NUM_VARS*NUM_SPIKES )
      SPIKES = -1.0

      ALLOCATE ( SPIKE_LOC(0:3,NUM_SPIKES), STAT = IERR )
      CALL MG_ASSERT ( IERR, 'MG_INIT: ALLOCATE ( SPIKE_LOC )', 4*NUM_SPIKES )
      SPIKE_LOC = -1

      ALLOCATE ( SOURCE_TOTAL(NUM_VARS), STAT = IERR )
      CALL MG_ASSERT ( IERR, 'MG_INIT: ALLOCATE ( SOURCE_TOTAL )', NUM_VARS )
      SOURCE_TOTAL = 0.0

      ! -------------------------------------------------------
      ! ROOT_PE generates and distributes SPIKES and SPIKE_LOC.
      ! Each GRIDi has a unique SPIKE value, but for each set,
      ! all in same location within the GRIDi array.
      ! Global location computed, PEs determines ownership.
      ! -------------------------------------------------------

      ! Determine global indices (excluding ghost)

      GLOBAL_NX = NX * NPX
      GLOBAL_NY = NY * NPY
      GLOBAL_NZ = NZ * NPZ

      MY_GLOBAL_NX_START = NX * MYPX + 1
      MY_GLOBAL_NY_START = NY * MYPY + 1
      MY_GLOBAL_NZ_START = NZ * MYPZ + 1

      MY_GLOBAL_NX_END   = MY_GLOBAL_NX_START + NX - 1
      MY_GLOBAL_NY_END   = MY_GLOBAL_NY_START + NY - 1
      MY_GLOBAL_NZ_END   = MY_GLOBAL_NZ_START + NZ - 1

      IF ( MYPE == ROOT_PE ) THEN
   
         CALL RANDOM_NUMBER ( SPIKES ( 1:NUM_VARS,1:NUM_SPIKES ) )
         SPIKES = SPIKES * GLOBAL_NX * GLOBAL_NY * GLOBAL_NZ

         ALLOCATE ( RSPIKE_LOC(3,NUM_SPIKES), STAT = IERR )
         CALL RANDOM_NUMBER ( RSPIKE_LOC )
      
         ! First spike set to problem dimension, inserted center of global grid.
         SPIKES(1,1) = REAL ( GLOBAL_NX * GLOBAL_NY * GLOBAL_NZ )

         SPIKE_LOC(0,1) = -1 
         SPIKE_LOC(1,1) = GLOBAL_NX / 2
         SPIKE_LOC(2,1) = GLOBAL_NY / 2
         SPIKE_LOC(3,1) = GLOBAL_NZ / 2

         ! Set additional spikes randomly about global grid

         DO I = 2, NUM_SPIKES
            SPIKE_LOC(0,I) = -1   ! Owning MPI rank will set to its MYPE.
            SPIKE_LOC(1,I) = INT ( RSPIKE_LOC(1,I) * GLOBAL_NX )
            SPIKE_LOC(2,I) = INT ( RSPIKE_LOC(2,I) * GLOBAL_NY )
            SPIKE_LOC(3,I) = INT ( RSPIKE_LOC(3,I) * GLOBAL_NZ )
         END DO

         DEALLOCATE ( RSPIKE_LOC )

      END IF ! ROOT_PE work.

#if defined _MG_MPI

      ! -----------------
      ! Distribute SPIKES
      ! -----------------

      CALL MPI_BCAST ( SPIKES, NUM_VARS*NUM_SPIKES, MG_MPI_REAL, ROOT_PE,  &
                       MPI_COMM_MG, IERR )
      CALL MG_ASSERT ( IERR, 'INIT: MPI_BCAST(SPIKES)', IERR )

      CALL MPI_BCAST ( SPIKE_LOC, 4*NUM_SPIKES, MPI_INTEGER, ROOT_PE,  &
                       MPI_COMM_MG, IERR )
      CALL MG_ASSERT ( IERR, 'INIT: MPI_BCAST(SPIKE_LOC)', IERR )

#endif

      ! Owning MPI rank determines converts the global location to its local array indices.

      DO I = 1, NUM_SPIKES

         XLOC = SPIKE_LOC ( 1, I ) ! Global values here.
         YLOC = SPIKE_LOC ( 2, I )
         ZLOC = SPIKE_LOC ( 3, I )

         IF ( ( MY_GLOBAL_NX_START <= XLOC ) .AND. ( XLOC <= MY_GLOBAL_NX_END ) .AND.  &
              ( MY_GLOBAL_NY_START <= YLOC ) .AND. ( YLOC <= MY_GLOBAL_NY_END ) .AND.  &
              ( MY_GLOBAL_NZ_START <= ZLOC ) .AND. ( ZLOC <= MY_GLOBAL_NZ_END ) ) THEN
   
            SPIKE_LOC ( 0, I ) = MYPE

            SPIKE_LOC ( 1, I ) = SPIKE_LOC ( 1, I ) - MY_GLOBAL_NX_START
            SPIKE_LOC ( 2, I ) = SPIKE_LOC ( 2, I ) - MY_GLOBAL_NY_START 
            SPIKE_LOC ( 3, I ) = SPIKE_LOC ( 3, I ) - MY_GLOBAL_NZ_START 

         ELSE

            ! Not owner, so set to -1 (to make obvious).

            SPIKE_LOC ( 0, I ) = -1

            SPIKE_LOC ( 1, I ) = -1
            SPIKE_LOC ( 2, I ) = -1
            SPIKE_LOC ( 3, I ) = -1

         END IF

      END DO

      RETURN
   
   END SUBROUTINE MG_INIT

!  ===================================================================================
 
   SUBROUTINE MG_PRINT_HEADER ( COMM_METHOD, STENCIL, IERR )

      IMPLICIT NONE

      ! Argument Declarations
      INTEGER, INTENT(IN)  ::       &
         COMM_METHOD, STENCIL

      INTEGER, INTENT(OUT) :: IERR

      !  Purpose
      !  =======
      !  Collate, process, and report performance results.

      ! Local Scalars
      CHARACTER(LEN=30) ::        &
         TEST_DATE
      CHARACTER(LEN=30) ::        &
         TEST_TIME

      INTEGER ::            &
         I,                              &
         ICLOCK_RATE,                    &
         IDUM,                           &
         LEN

      REAL(KIND=MG_REAL4) ::       &
         CLOCK_RES = 0.0

      INTEGER(KIND=MG_INT8), PARAMETER ::  &
         SIZE_OF_DATA = 8

      ! ---------------------
      ! Executable Statements
      ! ---------------------

      IERR = 0

      IF ( MYPE /= ROOT_PE ) &
         RETURN

      CALL DATE_AND_TIME ( TEST_DATE, TEST_TIME )

      WRITE(*,*)
      WRITE(*,100)
      WRITE(*,*) '          Mantevo miniapp MiniGhost experiment'
      WRITE(*,100)

#if defined _MG_MPI
      CLOCK_RES = REAL(MPI_WTICK ( ))

      WRITE(*,*)
      SELECT CASE ( COMM_METHOD )
         CASE ( COMM_METHOD_BSPMA )

            WRITE(*,*) 'Communication strategy: full message aggregation (COMM_METHOD_BSPMA)'

         CASE ( COMM_METHOD_SVAF )

            WRITE(*,*) 'Communication strategy: one variable at a time (COMM_METHOD_SVAF)'

         CASE DEFAULT

            WRITE(*,*) '** Warning ** Unknown communication strategy'

      END SELECT
#endif

      WRITE(*,*)

      SELECT CASE ( STENCIL )

         CASE ( STENCIL_NONE )

            WRITE(*,*) 'No computation inserted.'

         CASE ( STENCIL_2D5PT )

            WRITE(*,*) 'Computation: 5 pt difference stencil on a 2D grid (STENCIL_2D5PT)'

         CASE ( STENCIL_2D9PT )

            WRITE(*,*) 'Computation: 9 pt difference stencil on a 2D grid (STENCIL_2D9PT)'

         CASE ( STENCIL_3D7PT )

            WRITE(*,*) 'Computation: 7 pt difference stencil on a 3D grid (STENCIL_3D7PT)'

         CASE ( STENCIL_3D27PT )

            WRITE(*,*) 'Computation: 27 pt difference stencil on a 3D grid stencil (STENCIL_3D27PT)'

         CASE DEFAULT

            WRITE(*,*) '** Warning ** Unknown computation'

      END SELECT

      WRITE(*,*)
      WRITE(*,101) NX * NPX, NY * NPY, NZ * NPZ
      WRITE(*,102) NX, NY, NZ
      WRITE(*,*)
      WRITE(*,103) NUM_VARS
      WRITE(*,*)
      IF ( REPORT_DIFFUSION /= 0 ) THEN
         WRITE(*,104) REPORT_DIFFUSION, ERROR_TOL
      END IF
      WRITE(*,105) NUM_SUM_GRID, PERCENT_SUM
      WRITE(*,*)
      WRITE(*,110) NUM_TSTEPS
      WRITE(*,*)
#if defined _MG_MPI
      WRITE(*,120) NPX, NPY, NPZ
      WRITE(*,*)
      IF ( SCALING == SCALING_STRONG ) THEN   ! Not that it really matters.
         WRITE(*,*) 'MPI version, strong scaling'
      ELSE
         WRITE(*,*) 'MPI version, weak scaling'
      END IF
      WRITE(*,*)
      IF ( NUMPES == 1 ) THEN
         WRITE(*,121) TEST_TIME, TEST_DATE
      ELSE
         WRITE(*,122) NUMPES, TEST_TIME, TEST_DATE
      END IF
#elif defined _MG_SERIAL
      IF ( SCALING == SCALING_STRONG ) THEN   ! Not that it really matters.
         WRITE(*,123) 'Serial version, strong scaling', TEST_TIME, TEST_DATE
      ELSE
         WRITE(*,123) 'Serial version, weak scaling', TEST_TIME, TEST_DATE
      END IF
      WRITE(*,*)
#endif

      ! Format statements

 100  FORMAT ( ' =================================================' )

 101  FORMAT ( '      Global Grid Dimension: ', I8, ', ', I8, ', ', I8 )
 102  FORMAT ( '      Local Grid Dimension : ', I8, ', ', I8, ', ', I8 )

 103  FORMAT ( ' Number of variables: ', I2 )
 104  FORMAT ( ' Error reported every ', I3, ' time steps. Tolerance is ', 1PE8.2 )
 105  FORMAT ( ' Number of variables reduced each time step: ', I2, '; requested  ', I3, '%.')

 110  FORMAT ( '      Time steps: ', I6 )

 120  FORMAT ( '      Task grid: ', I5, ',', I5, ',', I5 )

 121  FORMAT ( ' 1 process executing', // &
             ' Program execution at ', A10, ' on ', A8, '.' )

 122  FORMAT ( I4, ' processes executing', // &
             ' Program execution at ', A10, ' on ', A8, '.' )
 123  FORMAT ( A32, ', ', A10, ' on ', A8, '.' )

   END SUBROUTINE MG_PRINT_HEADER

!  ===================================================================================

   SUBROUTINE MG_GRID_INIT ( IERR ) 

      INTEGER, INTENT(OUT) ::  &
         IERR                    ! Return status.

      ! ------------------
      ! Local Declarations
      ! ------------------

      REAL(KIND=MG_REAL) ::    &
         GSUM

      ! ---------------------
      ! Executable Statements
      ! ---------------------

      IERR = 0

      ! Allocate and initialize each GRID array. Set SOURCE_TOTAL to sum of elements.

      IF ( NUM_VARS > 0 ) THEN
         ALLOCATE ( GRID1( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID1 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID1, GSUM, IERR )
         SOURCE_TOTAL(1) = GSUM
      END IF

      IF ( NUM_VARS > 1 ) THEN
         ALLOCATE ( GRID2( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID2 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID2, GSUM, IERR )
         SOURCE_TOTAL(2) = GSUM
      END IF

      IF ( NUM_VARS > 2 ) THEN
         ALLOCATE ( GRID3( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID3 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID3, GSUM, IERR )
         SOURCE_TOTAL(3) = GSUM
      END IF

      IF ( NUM_VARS > 3 ) THEN
         ALLOCATE ( GRID4( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID4 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID4, GSUM, IERR )
         SOURCE_TOTAL(4) = GSUM
      END IF

      IF ( NUM_VARS > 4 ) THEN
         ALLOCATE ( GRID5( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID5 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID5, GSUM, IERR )
         SOURCE_TOTAL(5) = GSUM
      END IF

      IF ( NUM_VARS > 5 ) THEN
         ALLOCATE ( GRID6( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID6 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID6, GSUM, IERR )
         SOURCE_TOTAL(6) = GSUM
      END IF

      IF ( NUM_VARS > 6 ) THEN
         ALLOCATE ( GRID7( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID7 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID7, GSUM, IERR )
         SOURCE_TOTAL(7) = GSUM
      END IF

      IF ( NUM_VARS > 7 ) THEN
         ALLOCATE ( GRID8( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID8 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID8, GSUM, IERR )
         SOURCE_TOTAL(8) = GSUM
      END IF

      IF ( NUM_VARS > 8 ) THEN
         ALLOCATE ( GRID9( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID9 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID9, GSUM, IERR )
         SOURCE_TOTAL(9) = GSUM
      END IF

      IF ( NUM_VARS > 9 ) THEN
         ALLOCATE ( GRID10( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID10 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID10, GSUM, IERR )
         SOURCE_TOTAL(10) = GSUM
      END IF

      IF ( NUM_VARS > 10 ) THEN
         ALLOCATE ( GRID11( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID11 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID11, GSUM, IERR )
         SOURCE_TOTAL(11) = GSUM
      END IF

      IF ( NUM_VARS > 11 ) THEN
         ALLOCATE ( GRID12( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID12 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID12, GSUM, IERR )
         SOURCE_TOTAL(12) = GSUM
      END IF

      IF ( NUM_VARS > 12 ) THEN
         ALLOCATE ( GRID13( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID13 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID13, GSUM, IERR )
         SOURCE_TOTAL(13) = GSUM
      END IF

      IF ( NUM_VARS > 13 ) THEN
         ALLOCATE ( GRID14( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID14 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID14, GSUM, IERR )
         SOURCE_TOTAL(14) = GSUM
      END IF

      IF ( NUM_VARS > 14 ) THEN
         ALLOCATE ( GRID15( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID15 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID15, GSUM, IERR )
         SOURCE_TOTAL(15) = GSUM
      END IF

      IF ( NUM_VARS > 15 ) THEN
         ALLOCATE ( GRID16( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID16 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID16, GSUM, IERR )
         SOURCE_TOTAL(16) = GSUM
      END IF

      IF ( NUM_VARS > 16 ) THEN
         ALLOCATE ( GRID17( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID17 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID17, GSUM, IERR )
         SOURCE_TOTAL(17) = GSUM
      END IF

      IF ( NUM_VARS > 17 ) THEN
         ALLOCATE ( GRID18( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID18 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID18, GSUM, IERR )
         SOURCE_TOTAL(18) = GSUM
      END IF

      IF ( NUM_VARS > 18 ) THEN
         ALLOCATE ( GRID19( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID19 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID19, GSUM, IERR )
         SOURCE_TOTAL(19) = GSUM
      END IF

      IF ( NUM_VARS > 19 ) THEN
         ALLOCATE ( GRID20( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID20 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID20, GSUM, IERR )
         SOURCE_TOTAL(20) = GSUM
      END IF

      IF ( NUM_VARS > 20 ) THEN
         ALLOCATE ( GRID21( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID21 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID21, GSUM, IERR )
         SOURCE_TOTAL(21) = GSUM
      END IF

      IF ( NUM_VARS > 21 ) THEN
         ALLOCATE ( GRID22( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID22 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID22, GSUM, IERR )
         SOURCE_TOTAL(22) = GSUM
      END IF

      IF ( NUM_VARS > 22 ) THEN
         ALLOCATE ( GRID23( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID23 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID23, GSUM, IERR )
         SOURCE_TOTAL(23) = GSUM
      END IF

      IF ( NUM_VARS > 23 ) THEN
         ALLOCATE ( GRID24( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID24 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID24, GSUM, IERR )
         SOURCE_TOTAL(24) = GSUM
      END IF

      IF ( NUM_VARS > 24 ) THEN
         ALLOCATE ( GRID25( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID25 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID25, GSUM, IERR )
         SOURCE_TOTAL(25) = GSUM
      END IF

      IF ( NUM_VARS > 25 ) THEN
         ALLOCATE ( GRID26( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID26 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID26, GSUM, IERR )
         SOURCE_TOTAL(26) = GSUM
      END IF

      IF ( NUM_VARS > 26 ) THEN
            ALLOCATE ( GRID27( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID27 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID27, GSUM, IERR )
         SOURCE_TOTAL(27) = GSUM
      END IF

      IF ( NUM_VARS > 27 ) THEN
         ALLOCATE ( GRID28( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID28 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID28, GSUM, IERR )
         SOURCE_TOTAL(28) = GSUM
      END IF

      IF ( NUM_VARS > 28 ) THEN
         ALLOCATE ( GRID29( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID29 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID29, GSUM, IERR )
         SOURCE_TOTAL(29) = GSUM
      END IF

      IF ( NUM_VARS > 29 ) THEN
         ALLOCATE ( GRID30( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID30 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID30, GSUM, IERR )
         SOURCE_TOTAL(30) = GSUM
      END IF


      IF ( NUM_VARS > 30 ) THEN
         ALLOCATE ( GRID31( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID31 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID31, GSUM, IERR )
         SOURCE_TOTAL(31) = GSUM
      END IF

      IF ( NUM_VARS > 31 ) THEN
         ALLOCATE ( GRID32( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID32 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID32, GSUM, IERR )
         SOURCE_TOTAL(32) = GSUM
      END IF

      IF ( NUM_VARS > 32 ) THEN
         ALLOCATE ( GRID33( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID33 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID33, GSUM, IERR )
         SOURCE_TOTAL(33) = GSUM
      END IF

      IF ( NUM_VARS > 33 ) THEN
         ALLOCATE ( GRID34( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID34 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID34, GSUM, IERR )
         SOURCE_TOTAL(34) = GSUM
      END IF

      IF ( NUM_VARS > 34 ) THEN
         ALLOCATE ( GRID35( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID35 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID35, GSUM, IERR )
         SOURCE_TOTAL(35) = GSUM
      END IF

      IF ( NUM_VARS > 35 ) THEN
         ALLOCATE ( GRID36( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID36 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID36, GSUM, IERR )
         SOURCE_TOTAL(36) = GSUM
      END IF

      IF ( NUM_VARS > 36 ) THEN
         ALLOCATE ( GRID37( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID37 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID37, GSUM, IERR )
         SOURCE_TOTAL(37) = GSUM
      END IF

      IF ( NUM_VARS > 37 ) THEN
         ALLOCATE ( GRID38( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID38 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID38, GSUM, IERR )
         SOURCE_TOTAL(38) = GSUM
      END IF

      IF ( NUM_VARS > 38 ) THEN
         ALLOCATE ( GRID39( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID39 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID39, GSUM, IERR )
         SOURCE_TOTAL(39) = GSUM
      END IF

      IF ( NUM_VARS > 39 ) THEN
         ALLOCATE ( GRID40( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
         CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( GRID40 )', (NX+2)*(NY+2)*(NZ+2) )
         CALL MG_INIT_GRID ( GRID40, GSUM, IERR )
         SOURCE_TOTAL(40) = GSUM
      END IF

      IF ( NUM_VARS > 40 ) THEN
         IERR = -1
         CALL MG_ASSERT ( IERR, 'GRID_INIT: TOO MANY VARS', NUM_VARS )
      END IF

      ALLOCATE ( WORK( 0:NX+1, 0:NY+1, 0:NZ+1 ), STAT = IERR )
      CALL MG_ASSERT ( IERR, 'GRID_INIT: ALLOCATE ( WORK )', (NX+2)*(NY+2)*(NZ+2) )

      RETURN

   END SUBROUTINE MG_GRID_INIT

!  ===================================================================================

   SUBROUTINE MG_INSERT_SPIKE ( SPIKE_NUM, IERR )

      ! Insert heat source (SPIKES) into arrays. 
      ! This vlaue is added to the total heat applied to the variable.

      INTEGER, INTENT(IN) ::  &
         SPIKE_NUM

      INTEGER, INTENT(OUT) ::  &
         IERR                    ! Return status.

      ! ------------------
      ! Local Declarations
      ! ------------------

      INTEGER ::  &
         IX,      & ! Local index
         IY,      & ! Local index
         IZ,      & ! Local index
         I          ! Counter

      ! ---------------------
      ! Executable Statements
      ! ---------------------

      IERR = 0

      IF ( MYPE == ( SPIKE_LOC ( 0, SPIKE_NUM ) ) ) THEN

         IX = SPIKE_LOC ( 1, SPIKE_NUM )
         IY = SPIKE_LOC ( 2, SPIKE_NUM )
         IZ = SPIKE_LOC ( 3, SPIKE_NUM )

         IF ( NUM_VARS > 0 ) THEN
            GRID1( IX, IY, IZ ) = SPIKES( 1, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 1 ) THEN
            GRID2( IX, IY, IZ ) = SPIKES( 2, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 2 ) THEN
            GRID3( IX, IY, IZ ) = SPIKES( 3, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 3 ) THEN
            GRID4( IX, IY, IZ ) = SPIKES( 4, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 4 ) THEN
            GRID5( IX, IY, IZ ) = SPIKES( 5, SPIKE_NUM )
         END IF
   
         IF ( NUM_VARS > 5 ) THEN
            GRID6( IX, IY, IZ ) = SPIKES( 6, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 6 ) THEN
            GRID7( IX, IY, IZ ) = SPIKES( 7, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 7 ) THEN
            GRID8( IX, IY, IZ ) = SPIKES( 8, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 8 ) THEN
            GRID9( IX, IY, IZ ) = SPIKES( 9, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 9 ) THEN
            GRID10( IX, IY, IZ ) = SPIKES( 10, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 10 ) THEN
            GRID11( IX, IY, IZ ) = SPIKES( 11, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 11 ) THEN
            GRID12( IX, IY, IZ ) = SPIKES( 12, SPIKE_NUM )
         END IF
   
         IF ( NUM_VARS > 12 ) THEN
            GRID13( IX, IY, IZ ) = SPIKES( 13, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 13 ) THEN
            GRID14( IX, IY, IZ ) = SPIKES( 14, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 14 ) THEN
            GRID15( IX, IY, IZ ) = SPIKES( 15, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 15 ) THEN
            GRID16( IX, IY, IZ ) = SPIKES( 16, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 16 ) THEN
            GRID17( IX, IY, IZ ) = SPIKES( 17, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 17 ) THEN
            GRID18( IX, IY, IZ ) = SPIKES( 18, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 18 ) THEN
            GRID19( IX, IY, IZ ) = SPIKES( 19, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 19 ) THEN
            GRID20( IX, IY, IZ ) = SPIKES( 20, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 20 ) THEN
            GRID21( IX, IY, IZ ) = SPIKES( 21, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 21 ) THEN
            GRID22( IX, IY, IZ ) = SPIKES( 22, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 22 ) THEN
            GRID23( IX, IY, IZ ) = SPIKES( 23, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 23 ) THEN
            GRID24( IX, IY, IZ ) = SPIKES( 24, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 24 ) THEN
            GRID25( IX, IY, IZ ) = SPIKES( 25, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 25 ) THEN
            GRID26( IX, IY, IZ ) = SPIKES( 26, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 26 ) THEN
            GRID27( IX, IY, IZ ) = SPIKES( 27, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 27 ) THEN
            GRID28( IX, IY, IZ ) = SPIKES( 28, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 28 ) THEN
            GRID29( IX, IY, IZ ) = SPIKES( 29, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 29 ) THEN
            GRID30( IX, IY, IZ ) = SPIKES( 30, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 30 ) THEN
            GRID31( IX, IY, IZ ) = SPIKES( 31, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 31 ) THEN
            GRID32( IX, IY, IZ ) = SPIKES( 32, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 32 ) THEN
            GRID33( IX, IY, IZ ) = SPIKES( 33, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 33 ) THEN
            GRID34( IX, IY, IZ ) = SPIKES( 34, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 34 ) THEN
            GRID35( IX, IY, IZ ) = SPIKES( 35, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 35 ) THEN
            GRID36( IX, IY, IZ ) = SPIKES( 36, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 36 ) THEN
            GRID37( IX, IY, IZ ) = SPIKES( 37, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 37 ) THEN
            GRID38( IX, IY, IZ ) = SPIKES( 38, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 38 ) THEN
            GRID39( IX, IY, IZ ) = SPIKES( 39, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 39 ) THEN
            GRID40( IX, IY, IZ ) = SPIKES( 40, SPIKE_NUM )
         END IF

         IF ( NUM_VARS > 40 ) THEN
            IERR = -1
            CALL MG_ASSERT ( IERR, 'GRID_INIT: TOO MANY VARS', NUM_VARS )
         END IF
      END IF

      IF ( MYPE == ROOT_PE ) THEN
         DO I = 1, NUM_VARS
            SOURCE_TOTAL( I ) = SOURCE_TOTAL( I ) + SPIKES( I, SPIKE_NUM )
         END DO
      END IF

   END SUBROUTINE MG_INSERT_SPIKE

!  ===================================================================================

   SUBROUTINE MG_GRID_DEALLOC ( IERR )
   
      USE MG_CONSTANTS_MOD
   
      IMPLICIT NONE
   
      ! Variable Declarations
   
      INTEGER :: &
         IERR                            ! Return status
   
      ! ---------------------
      ! Executable Statements
      ! ---------------------
   
      IERR = 0
   
      IF ( ALLOCATED ( GRID1 )  ) DEALLOCATE ( GRID1 )
      IF ( ALLOCATED ( GRID2 )  ) DEALLOCATE ( GRID2 )
      IF ( ALLOCATED ( GRID3 )  ) DEALLOCATE ( GRID3 )
      IF ( ALLOCATED ( GRID4 )  ) DEALLOCATE ( GRID4 )
      IF ( ALLOCATED ( GRID5 )  ) DEALLOCATE ( GRID5 )
      IF ( ALLOCATED ( GRID6 )  ) DEALLOCATE ( GRID6 )
      IF ( ALLOCATED ( GRID7 )  ) DEALLOCATE ( GRID7 )
      IF ( ALLOCATED ( GRID8 )  ) DEALLOCATE ( GRID8 )
      IF ( ALLOCATED ( GRID9 )  ) DEALLOCATE ( GRID9 )
      IF ( ALLOCATED ( GRID10 ) ) DEALLOCATE ( GRID10 )
   
      IF ( ALLOCATED ( GRID11 ) ) DEALLOCATE ( GRID11 )
      IF ( ALLOCATED ( GRID12 ) ) DEALLOCATE ( GRID12 )
      IF ( ALLOCATED ( GRID13 ) ) DEALLOCATE ( GRID13 )
      IF ( ALLOCATED ( GRID14 ) ) DEALLOCATE ( GRID14 )
      IF ( ALLOCATED ( GRID15 ) ) DEALLOCATE ( GRID15 )
      IF ( ALLOCATED ( GRID16 ) ) DEALLOCATE ( GRID16 )
      IF ( ALLOCATED ( GRID17 ) ) DEALLOCATE ( GRID17 )
      IF ( ALLOCATED ( GRID18 ) ) DEALLOCATE ( GRID18 )
      IF ( ALLOCATED ( GRID19 ) ) DEALLOCATE ( GRID19 )
      IF ( ALLOCATED ( GRID20 ) ) DEALLOCATE ( GRID20 )
   
      IF ( ALLOCATED ( GRID21 ) ) DEALLOCATE ( GRID21 )
      IF ( ALLOCATED ( GRID22 ) ) DEALLOCATE ( GRID22 )
      IF ( ALLOCATED ( GRID23 ) ) DEALLOCATE ( GRID23 )
      IF ( ALLOCATED ( GRID24 ) ) DEALLOCATE ( GRID24 )
      IF ( ALLOCATED ( GRID25 ) ) DEALLOCATE ( GRID25 )
      IF ( ALLOCATED ( GRID26 ) ) DEALLOCATE ( GRID26 )
      IF ( ALLOCATED ( GRID27 ) ) DEALLOCATE ( GRID27 )
      IF ( ALLOCATED ( GRID28 ) ) DEALLOCATE ( GRID28 )
      IF ( ALLOCATED ( GRID29 ) ) DEALLOCATE ( GRID29 )
      IF ( ALLOCATED ( GRID30 ) ) DEALLOCATE ( GRID30 )
   
      IF ( ALLOCATED ( GRID31 ) ) DEALLOCATE ( GRID31 )
      IF ( ALLOCATED ( GRID32 ) ) DEALLOCATE ( GRID32 )
      IF ( ALLOCATED ( GRID33 ) ) DEALLOCATE ( GRID33 )
      IF ( ALLOCATED ( GRID34 ) ) DEALLOCATE ( GRID34 )
      IF ( ALLOCATED ( GRID35 ) ) DEALLOCATE ( GRID35 )
      IF ( ALLOCATED ( GRID36 ) ) DEALLOCATE ( GRID36 )
      IF ( ALLOCATED ( GRID37 ) ) DEALLOCATE ( GRID37 )
      IF ( ALLOCATED ( GRID38 ) ) DEALLOCATE ( GRID38 )
      IF ( ALLOCATED ( GRID39 ) ) DEALLOCATE ( GRID39 )
      IF ( ALLOCATED ( GRID40 ) ) DEALLOCATE ( GRID40 )
   
   END SUBROUTINE MG_GRID_DEALLOC

!  ===================================================================================

   SUBROUTINE MG_ASSERT ( IERR, ERROR_MSG, INFO )

      USE MG_CONSTANTS_MOD

      IMPLICIT NONE

      CHARACTER(LEN=*), INTENT(IN) ::    &
         ERROR_MSG

      INTEGER, INTENT(IN) ::             &
         IERR,                           & ! Error code.
         INFO

      ! ---------------------
      ! Executable Statements
      ! ---------------------

#if defined _MG_MPI

      CHARACTER*(MPI_MAX_ERROR_STRING) STRING
      INTEGER RESULTLEN, IERROR

      IF ( IERR /= 0 ) THEN
         WRITE(*,80) MYPE, TRIM(ERROR_MSG), IERR, INFO
         call MPI_ERROR_STRING(IERR, STRING, RESULTLEN, IERROR)
         WRITE(*,81) STRING
         CALL MPI_ABORT ( MPI_COMM_MG, -1, IERR )
      END IF

 80   FORMAT ( '** Error ** [pe ', I5, '] ', A40, '; CODE = ', I7, &
               '. Additional info:', I4 )
 81   FORMAT ( 'MPI error message: ', A80)

#else

      IF ( IERR /= 0 ) THEN
         WRITE(*,90) TRIM(ERROR_MSG), IERR, INFO
         STOP
      END IF

#endif

 90   FORMAT ( '** Error ** ', A40, '; CODE = ', I7, '. Additional info:', I4 )


   END SUBROUTINE MG_ASSERT

!  ===================================================================================

   SUBROUTINE MG_INIT_GRID( GRID, GSUM, IERR )

      IMPLICIT NONE

      INTEGER, INTENT(OUT) ::  &
         IERR                    ! Return status.

      REAL(KIND=MG_REAL), INTENT(OUT) :: &
         GSUM                    ! Sum of the GRID values upon return.

      REAL(KIND=MG_REAL), DIMENSION(0:NX+1, 0:NY+1, 0:NZ+1), INTENT(OUT) :: &
         GRID

      ! ---------------
      ! Local Variables
      ! ---------------

      INTEGER :: I, J, K
      REAL(KIND=MG_REAL) :: LSUM

      ! ---------------------
      ! Executable Statements
      ! ---------------------

      ! Explicit loops enable first touch affinity for OpenMP.

      IF ( DEBUG_GRID == 1 ) THEN

!$OMP PARALLEL DO
         DO K = 0, NZ+1
            DO J = 0, NY+1
               DO I = 0, NX+1
                  GRID(I, J, K) = 0.0
               END DO
            END DO
         END DO
!$OMP END PARALLEL DO
         GSUM = 0.0

      ELSE

         LSUM = 0.0
!$OMP PARALLEL DO
         DO K = 0, NZ+1
            DO J = 0, NY+1
               DO I = 0, NX+1
                  CALL RANDOM_NUMBER( GRID(I, J, K) )
                  LSUM = LSUM + GRID(I, J, K )
               END DO
            END DO
         END DO
!$OMP END PARALLEL DO

#if defined _MG_MPI
         CALL MPI_ALLREDUCE ( LSUM, GSUM, 1, MG_MPI_REAL, MPI_SUM, MPI_COMM_MG, IERR )
         CALL MG_ASSERT ( IERR, 'MG_INIT_GRID : MPI_ALLREDUCE', 1 )
#elif defined _MG_SERIAL
         GSUM = LSUM
#endif

      END IF

      RETURN

   END SUBROUTINE MG_INIT_GRID

!  ===================================================================================

   DOUBLE PRECISION FUNCTION MG_TIMER ()

#if defined _MG_MPI
      include 'mpif.h'

      MG_TIMER = MPI_WTIME ()

#else
      INTEGER COUNT_1, COUNT_RATE, COUNT_MAX

      CALL SYSTEM_CLOCK (COUNT_1, COUNT_RATE, COUNT_MAX)
      MG_TIMER = COUNT_1 * 1.0 / COUNT_RATE

#endif

      RETURN
 
   END FUNCTION MG_TIMER
!  ===================================================================================

   DOUBLE PRECISION FUNCTION MG_COMPUTE_STDDEV ( VALUES, MEAN )

      DOUBLE PRECISION :: &
         VALUES(*), MEAN

      INTEGER :: I 
      DOUBLE PRECISION :: TMP

      TMP = 0.0
      DO I = 1, NUMPES
         TMP = TMP + ( VALUES(I) - MEAN )**2
      END DO

      MG_COMPUTE_STDDEV = SQRT ( TMP / REAL( NUMPES ))

      RETURN

   END FUNCTION MG_COMPUTE_STDDEV

!  ===================================================================================

END MODULE MG_UTILS_MOD
