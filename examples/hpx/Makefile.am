examplesdir          = $(datarootdir)/doc/@PACKAGE@

examples_PROGRAMS    = fibonaccihpx pingponghpx allreduce countdown

fibonaccihpx_SOURCES = fibonacci.c
pingponghpx_SOURCES  = pingpong.c
allreduce_SOURCES    = allreduce.c
countdown_SOURCES    = countdown.c

allreduce_LDADD      = $(top_builddir)/libhpx/libhpx.la 
fibonaccihpx_LDADD   = $(top_builddir)/libhpx/libhpx.la
pingponghpx_LDADD    = $(top_builddir)/libhpx/libhpx.la
countdown_LDADD      = $(top_builddir)/libhpx/libhpx.la

AM_CPPFLAGS          = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200112L

AM_CFLAGS            = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR)
AM_CFLAGS           += @PHOTON_CFLAGS@ @MPI_CFLAGS@ @CRYPTO_CFLAGS@
AM_CFLAGS           += @HWLOC_CFLAGS@ @PAPI_CFLAGS@ @TCMALLOC_CFLAGS@
AM_CFLAGS           += @GLIB_CFLAGS@
AM_CFLAGS           += -std=c99

AM_LDFLAGS          = -static

# allreduce uses MPI directly
allreduce_LDADD    += @MPI_LIBS@
