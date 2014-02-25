####################
# Makefile for HPX #
####################

noinst_LTLIBRARIES	     = libtransport.la

libtransport_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libhpx
libtransport_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libtransport_la_SOURCES  = smp/transport.c
libtransport_la_LIBADD   =

if HAVE_MPI
libtransport_la_CFLAGS  += @MPI_FLAGS@
libtransport_la_SOURCES += mpi/transport.c
libtransport_la_LIBADD  += @MPI_LIBS@
endif

if HAVE_PHOTON
libtransport_la_CFLAGS  += @PHOTON_CFLAGS@
libtransport_la_SOURCES += photon/transport.c
libtransport_la_LIBADD  += @PHOTON_LIBS@
endif
