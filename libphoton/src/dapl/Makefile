HARDWARE=dapl

CC=$(MPICC)
LD=ld

XSP=1

ifdef XSP
LIBphotonA=libphoton_xsp.a
else
ifdef DEBUG
LIBphotonA=libphoton_debug.a
else
LIBphotonA=libphoton.a
endif
endif

LIBphotonSO=libphoton.so

CFLAGS=-fPIC -finline-functions -Wall
LDFLAGS=-shared
#INCLUDES=-I/usr/OFED-1.5/include -I. -I../../include -I../dapl_util -I/home/fernandes/dev/workspace/xsp/libxsp_client
INCLUDES=-I. -I../../include -I../dapl_util -I/home/fernandes/dev/workspace/xsp/libxsp_client

UTILOBJS=../dapl_util/htable.o ../dapl_util/buffertable.o

ifeq ($(HARDWARE),dapl)
LIBphotonOBJS=dapl.o dapl_buffer.o dapl_remote_buffer.o dapl_rdma_info_ledger.o dapl_rdma_FIN_ledger.o wrapper.o
endif

ifeq ($(HARDWARE),mpi)
LIBphotonOBJS=mpi.o
endif

ifeq ($(HARDWARE),gm)
LIBphotonOBJS=
endif

ifdef DEBUG
CFLAGS+=-g
CFLAGS+=-DDEBUG
CFLAGS+=-DCALLTRACE
else
CFLAGS+=-O3
endif

ifdef XSP
CFLAGS+=-DWITH_XSP
endif

all: libphoton.a
debug:	libphoton_debug.a

libphoton.so: $(LIBphotonOBJS)
	$(LD) $(LDFLAGS) -o $(LIBphotonSO) $(LIBphotonOBJS) $(UTILOBJS)
	mv $(LIBphotonSO) ../..

libphoton.a: $(LIBphotonOBJS)
	ar rc $(LIBphotonA) $(LIBphotonOBJS) $(UTILOBJS)
	ranlib $(LIBphotonA)
	mv $(LIBphotonA) ../..

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

clean:
	rm -f $(LIBphotonA) $(LIBphotonSO) $(LIBphotonOBJS) *.o
