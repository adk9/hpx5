HARDWARE=verbs

CC=$(MPICC)
LD=ld

ifdef XSP
ifdef MULTITHREADED
LIBphotonA=libphoton_verbs_xsp_mt.a
else
LIBphotonA=libphoton_verbs_xsp.a
endif
else
ifdef DEBUG
LIBphotonA=libphoton_verbs_debug.a
else
LIBphotonA=libphoton_verbs.a
endif
endif

LIBphotonSO=libphoton_verbs.so

CFLAGS=-fPIC -finline-functions -Wall
LDFLAGS=-shared
INCLUDES=-I. -I../../include -I../verbs_util -I../../../../xsp/libxsp_client

UTILOBJS=../verbs_util/htable.o ../verbs_util/buffertable.o

ifeq ($(HARDWARE),verbs)
LIBphotonOBJS=verbs.o verbs_buffer.o verbs_remote_buffer.o verbs_rdma_info_ledger.o verbs_rdma_FIN_ledger.o wrapper.o
endif

ifeq ($(HARDWARE),mpi)
LIBphotonOBJS=mpi.o
endif

ifeq ($(HARDWARE),gm)
LIBphotonOBJS=
endif

ifdef DEBUG
CFLAGS+=-g -O0 -DDEBUG -DCALLTRACE
else
CFLAGS+=-O3
endif

ifdef XSP
CFLAGS+=-DWITH_XSP
endif

ifdef MULTITHREADED
CFLAGS+=-DPHOTON_MULTITHREADED
endif

all: libphoton.a
debug:	libphoton_debug.a

libphoton.so: $(LIBphotonOBJS)
	$(LD) $(LDFLAGS) -o $(LIBphotonSO) $(LIBphotonOBJS) $(UTILOBJS)
	mv $(LIBphotonSO) ../../lib

libphoton.a: $(LIBphotonOBJS)
	ar rc $(LIBphotonA) $(LIBphotonOBJS) $(UTILOBJS)
	ranlib $(LIBphotonA)
	mv $(LIBphotonA) ../../lib

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

clean:
	rm -f $(LIBphotonA) $(LIBphotonSO) $(LIBphotonOBJS) *.o

pingpong: srq_pingpong.c pingpong.c
	gcc -D_GNU_SOURCE -o srq_pingpong srq_pingpong.c pingpong.c -L/usr/ofed-1.3/lib64 -libverbs
