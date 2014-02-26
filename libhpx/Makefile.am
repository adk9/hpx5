####################
# Makefile for HPX #
####################

# The main libhpx library
lib_LTLIBRARIES		= libhpx.la

# LT Utility libraries
noinst_LTLIBRARIES	= libtransport.la libplatform.la

libhpx_la_SOURCES	= hpx.c locks.c locality.c scheduler.c parcel.c action.c \
					  thread.c network.c future.c

if CPU_X86_64
libhpx_la_SOURCES  += arch/x86_64/transfer.S arch/x86_64/asm.S
endif

libhpx_la_CPPFLAGS  = -I$(top_srcdir)/include

libhpx_la_CFLAGS	= $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99 \
					  @CRYPTO_CFLAGS@ @HWLOC_CFLAGS@ @PAPI_CFLAGS@ \
					  @TCMALLOC_CFLAGS@ @GLIB_CFLAGS@ @PMI_CFLAGS@ 

libhpx_la_LIBADD	= $(top_builddir)/libsync/libsync.la \
	      			   libtransport.la libplatform.la \
					  -lpthread @CRYPTO_LIBS@ @HWLOC_LIBS@ @PAPI_LIBS@ \
					  @TCMALLOC_LIBS@ @GLIB_LIBS@ @PMI_LIBS@


# libtransport files and flags
libtransport_la_CPPFLAGS = -I$(top_srcdir)/include
libtransport_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
libtransport_la_SOURCES  = smp/transport.c
libtransport_la_LIBADD   =

if HAVE_MPI
libtransport_la_CFLAGS  += @MPI_CFLAGS@
libtransport_la_SOURCES += mpi/transport.c
libtransport_la_LIBADD  += @MPI_LIBS@
endif

if HAVE_PHOTON
libtransport_la_CFLAGS  += @PHOTON_CFLAGS@
libtransport_la_SOURCES += photon/transport.c
libtransport_la_LIBADD  += @PHOTON_LIBS@
endif

# libplatform files and flags
libplatform_la_CPPFLAGS  = -I$(top_srcdir)/include
libplatform_la_CFLAGS	 = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99

if OS_LINUX
libplatform_la_CPPFLAGS += -D_POSIX_C_SOURCE=200809L
libplatform_la_SOURCES	 = linux/time.c
libplatform_la_LIBADD	 = -lrt
endif
