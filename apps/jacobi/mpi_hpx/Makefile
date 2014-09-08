CC = mpicc
F90 = gfortran
CFLAGS = -g
FFLAGS = -g
INCLUDES = -I/home/manderson/hpx5_gitlab/include
LIBS = -L/home/manderson/hpx5_gitlab/lib -lhpx -lrt -lm -lgfortran

OBJ = allgather.o alltoall.o bcast.o gatherv.o logging.o mpi_wtime.o reduce.o sendrecv.o allreduce.o alltoallv.o gather.o main.o mpi_system.o p2p.o scatterv.o jacobi_mpiomp.o #test.o
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

