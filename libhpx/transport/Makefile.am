# LT Utility libraries
noinst_LTLIBRARIES	= libtransport.la

# libtransport files and flags
libtransport_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(top_srcdir)/libhpx/network/
libtransport_la_CFLAGS    =
libtransport_la_SOURCES   = smp.c transport.c registration.c progress.c
libtransport_la_LDFLAGS   =
libtransport_la_LIBADD    =

if HAVE_MPI
libtransport_la_CFLAGS   += @MPI_CFLAGS@
libtransport_la_SOURCES  += mpi.c
libtransport_la_LIBADD   += @MPI_LIBS@
endif

if HAVE_PHOTON
libtransport_la_SOURCES  += photon.c
endif

if HAVE_PORTALS
libtransport_la_CFLAGS   += @PORTALS_CFLAGS@
libtransport_la_SOURCES  += portals.c
libtransport_la_LIBADD   += @PORTALS_LIBS@
endif

