            subroutine jacobi
            implicit none
            include 'mpif.h'

            integer ngrid,ierr,np,myid,tmp
            integer nbr_down, nbr_up
            integer status(3)
            print*,' HELLO WORLD '

            call mpi_comm_size(mpi_comm_world,np,ierr)
            call mpi_comm_rank(mpi_comm_world,myid,ierr) 

            if (myid .eq. 0 ) then
              ngrid = 59
            endif

            call mpi_bcast(ngrid,1,mpi_integer,0,mpi_comm_world,ierr)
            print*,' TEST bcast ',ngrid

            nbr_down = mpi_proc_null
            nbr_up   = mpi_proc_null
            if (myid >      0) nbr_down = myid - 1
            if (myid < np - 1) nbr_up   = myid + 1

            tmp = 0

            call mpi_sendrecv(myid,1,mpi_integer,nbr_down,314, &
                      tmp,1,mpi_integer,nbr_up  ,314, &
                      mpi_comm_world,status,ierr)

            print*,' TEST myid ',myid,' recv ',tmp

            end subroutine
