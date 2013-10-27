examplesdir			= $(datarootdir)/doc/@PACKAGE@

examples_PROGRAMS	= fibonacci fibsmarter
fibonacci_SOURCES	= fibonacci.c
fibonacci_LDADD		= $(top_builddir)/src/libhpx.la $(top_builddir)/libsync/libsync.la

fibsmarter_SOURCES  = fibsmarter.c
fibsmarter_LDADD    = $(top_builddir)/src/libhpx.la $(top_builddir)/libsync/libsync.la

AM_CPPFLAGS			= -I$(top_srcdir)/include

AM_CFLAGS			= $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR)
AM_CFLAGS		   += @PHOTON_CFLAGS@ @MPI_CFLAGS@ @CRYPTO_CFLAGS@
AM_CFLAGS		   += @HWLOC_CFLAGS@ @PAPI_CFLAGS@ @TCMALLOC_CFLAGS@
AM_CFLAGS		   += -std=c99

AM_LDFLAGS          = -static
