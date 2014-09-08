            subroutine jacobi
            implicit none
            include 'mpif.h'

            integer ngrid,ierr,np,myid
            print*,' HELLO WORLD '

            call mpi_comm_size(mpi_comm_world,np,ierr)
            call mpi_comm_rank(mpi_comm_world,myid,ierr) 

            if (myid .eq. 0 ) then
              ngrid = 59
            endif

            call mpi_bcast(ngrid,1,mpi_integer,0,mpi_comm_world,ierr)
            print*,' TEST bcast ',ngrid


            end subroutine
