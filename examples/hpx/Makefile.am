examplesdir          = $(datarootdir)/doc/@PACKAGE@

examples_PROGRAMS    = fibonaccihpx pingponghpx allreduce

fibonaccihpx_SOURCES = fibonacci.c
pingponghpx_SOURCES  = pingpong.c
allreduce_SOURCES    = allreduce.c

allreduce_LDADD      = $(top_builddir)/src/libhpx.la 
fibonaccihpx_LDADD   = $(top_builddir)/src/libhpx.la
pingponghpx_LDADD    = $(top_builddir)/src/libhpx.la

AM_CPPFLAGS          = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L

AM_CFLAGS            = $(HPX_PEDANTIC) $(HPX_W_ALL)
AM_CFLAGS           += @PHOTON_CFLAGS@ @MPI_CFLAGS@ @CRYPTO_CFLAGS@
AM_CFLAGS           += @HWLOC_CFLAGS@ @PAPI_CFLAGS@ @TCMALLOC_CFLAGS@
AM_CFLAGS           += @GLIB_CFLAGS@
AM_CFLAGS           += -std=c99

AM_LDFLAGS          = -static

# allreduce uses MPI directly
allreduce_LDADD     += @MPI_LIBS@
