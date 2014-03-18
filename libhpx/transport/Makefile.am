# LT Utility libraries
noinst_LTLIBRARIES	= libtransport.la

# libtransport files and flags
libtransport_la_CPPFLAGS = -I$(top_srcdir)/include
libtransport_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libtransport_la_SOURCES  = transport.c smp.c
libtransport_la_LIBADD   =

if HAVE_MPI
libtransport_la_CFLAGS  += @MPI_CFLAGS@
libtransport_la_SOURCES += mpi.c
libtransport_la_LIBADD  += @MPI_LIBS@
endif

if HAVE_PHOTON
libtransport_la_CFLAGS  += @PHOTON_CFLAGS@
libtransport_la_SOURCES += photon.c
libtransport_la_LIBADD  += @PHOTON_LIBS@
endif

