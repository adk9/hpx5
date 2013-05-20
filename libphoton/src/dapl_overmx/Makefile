HARDWARE=dapl_overmx

CC=$(MPICC)
LD=ld
ifdef DEBUG
LIBPHOTONA=libphoton_debug.a
else
LIBPHOTONA=libphoton.a
endif
LIBPHOTONSO=libphoton.so

CFLAGS=-fPIC -finline-functions -Wall
LDFLAGS=-shared
INCLUDES=-I/opt/dapl-mx/include -I. -I../../include -I../$(HARDWARE)_util -I/opt/mx/include

UTILOBJS=../$(HARDWARE)_util/htable.o ../$(HARDWARE)_util/buffertable.o

ifeq ($(HARDWARE),dapl_overmx)
LIBPHOTONOBJS=dapl.o dapl_buffer.o dapl_remote_buffer.o dapl_rdma_info_ledger.o dapl_rdma_FIN_ledger.o wrapper.o
endif

ifeq ($(HARDWARE),dapl)
LIBPHOTONOBJS=dapl.o dapl_buffer.o dapl_remote_buffer.o dapl_rdma_info_ledger.o dapl_rdma_FIN_ledger.o wrapper.o
endif

ifeq ($(HARDWARE),mpi)
LIBPHOTONOBJS=mpi.o
endif

ifeq ($(HARDWARE),gm)
LIBPHOTONOBJS=
endif

ifdef DEBUG
CFLAGS+=-g
CFLAGS+=-DDEBUG
CFLAGS+=-DCALLTRACE
else
CFLAGS+=-O3
endif

all: libphoton.a
debug:	libphoton_debug.a

libphoton.so: $(LIBPHOTONOBJS)
	$(LD) $(LDFLAGS) -o $(LIBPHOTONSO) $(LIBPHOTONOBJS) $(UTILOBJS)
	mv $(LIBPHOTONSO) ../..

libphoton.a: $(LIBPHOTONOBJS)
	ar rc $(LIBPHOTONA) $(LIBPHOTONOBJS) $(UTILOBJS)
	ranlib $(LIBPHOTONA)
	mv $(LIBPHOTONA) ../..

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

clean:
	rm -f $(LIBPHOTONA) $(LIBPHOTONSO) $(LIBPHOTONOBJS) *.o
