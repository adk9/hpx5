CC = mpicc
F90 = gfortran
CFLAGS = -g
FFLAGS = -g
INCLUDES = -I/home/manderson/hpx5_gitlab/include
LIBS = -L/home/manderson/hpx5_gitlab/lib -lhpx -lrt -lm

OBJ = allgather.c alltoall.c bcast.c gatherv.c logging.c mpi_wtime.c reduce.c sendrecv.c allreduce.c alltoallv.c gather.c main.c mpi_system.c p2p.c scatterv.c test.f90
TARGET = jacobi

.PHONY: clean help

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ) $(LIBS) 

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDES)

%.o:%.f90
	$(F90) $(FFLAGS) -c $< -o $@ 

clean:
	rm -f *.o $(TARGET) *~
help:
	@echo "Usage: ./$(TARGET) [nDoms] [nx] [maxcycles] [nCores]"

