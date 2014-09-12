CC = mpicc
F90 = gfortran
CFLAGS = -g -static
FFLAGS = -g
INCLUDES = -I../../../include -I../../mpi/
LIBS = -L../../mpi/.libs/ -lhpxmpi -L../../../libhpx/.libs/ -lhpx -lrt -lm -lgfortran

OBJ = main.o jacobi_mpiomp.o #test.o
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

