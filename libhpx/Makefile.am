####################
# Makefile for HPX #
####################

lib_LTLIBRARIES		= libhpx.la

libhpx_la_SOURCES	= placeholder.c

libhpx_la_CPPFLAGS  = -I$(top_srcdir)/include

libhpx_la_CFLAGS	= $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) \
					  @CRYPTO_CFLAGS@ @MPI_CFLAGS@ @PHOTON_CFLAGS@ \
					  @HWLOC_CFLAGS@ @PAPI_CFLAGS@ @TCMALLOC_CFLAGS@ \
					  @GLIB_CFLAGS@ @PMI_CFLAGS@ -std=c99  

libhpx_la_LIBADD	= $(top_builddir)/libsync/libsync.la @MPI_LIBS@ \
					  @PHOTON_LIBS@ @CRYPTO_LIBS@ @HWLOC_LIBS@ @PAPI_LIBS@ \
					  @TCMALLOC_LIBS@ @GLIB_LIBS@ @PMI_LIBS@ @PLATFORM_LIBS@ 
