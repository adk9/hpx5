MPIDIR=/opt/openmpi/1.6.4
#CC=gcc
CC=mpicc
MPIF=${MPIDIR}/bin/mpif90

INCLUDES=-I${MPIDIR}/include/ -I../common -I../libphoton/include/ -I../libphoton/src/verbs_util/
LIBS=-L${MPIDIR}/lib/ -lm -lmpi -libverbs
CFLAGS=-O3 -Wall

LIBS+=-L../libphoton/lib -lphoton_verbs -libverbs
INCLUDES+=-I../libphoton
CFLAGS+=-DUSE_photon

PHOTON_TEST_OBJS=photon_test.o kernel.o

all: photon_test

photon_test: $(PHOTON_TEST_OBJS) 
	$(CC) $(CFLAGS) -o photon_test $(PHOTON_TEST_OBJS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

clean:
	rm $(PHOTON_TEST_OBJS) photon_test
