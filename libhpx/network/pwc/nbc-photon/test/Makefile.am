sources1 = simple_ibcast.c 
sources2 = simple_ibcast_group.c
sources3 = simple_iallreduce.c

TESTS   = simple_ibcast simple_ibcastg simple_iallreduce

noinst_PROGRAMS = $(TESTS)

simple_ibcast_SOURCES = $(sources1)
#simple_ibcast_LDADD= ../libnbc.la -lmpi ../../../../../contrib/photon/src/libphoton.la  
simple_ibcast_LDADD= ../libnbc.la -lmpi ../../../../../contrib/photon/lib/libphoton_mpiexch.la  
simple_ibcast_CPPFLAGS = -I$(top_srcdir)/include -I../ $(LIBHPX_CPPFLAGS)
simple_ibcast_CFLAGS   = $(LIBHPX_CFLAGS) -DHAVE_MPI

simple_ibcastg_SOURCES = $(sources2)
#simple_ibcastg_LDADD= ../libnbc.la -lmpi ../../../../../contrib/photon/src/libphoton.la 
simple_ibcastg_LDADD= ../libnbc.la -lmpi ../../../../../contrib/photon/lib/libphoton_mpiexch.la
simple_ibcastg_CPPFLAGS = -I$(top_srcdir)/include -I../ $(LIBHPX_CPPFLAGS)
simple_ibcastg_CFLAGS   = $(LIBHPX_CFLAGS) -DHAVE_MPI

simple_iallreduce_SOURCES= $(sources3)
#simple_iallreduce_LDADD= ../libnbc.la -lmpi ../../../../../contrib/photon/src/libphoton.la 
simple_iallreduce_LDADD= ../libnbc.la -lmpi ../../../../../contrib/photon/lib/libphoton_mpiexch.la
simple_iallreduce_CPPFLAGS = -I$(top_srcdir)/include -I../ $(LIBHPX_CPPFLAGS)
simple_iallreduce_CFLAGS   = $(LIBHPX_CFLAGS) -DHAVE_MPI
