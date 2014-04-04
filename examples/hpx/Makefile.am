examplesdir          = $(datarootdir)/doc/@PACKAGE@

noinst_LTLIBRARIES     = libexamples.la
libexamples_la_SOURCES = debug.c
libexamples_la_CFLAGS  = -O0 -g -std=c99 

examples_PROGRAMS    = fibonaccihpx pingponghpx allreduce countdown seqspawn \
                       parspawn future

fibonaccihpx_SOURCES = fibonacci.c
pingponghpx_SOURCES  = pingpong.c
allreduce_SOURCES    = allreduce.c
countdown_SOURCES    = countdown.c
seqspawn_SOURCES     = seqspawn.c
parspawn_SOURCES     = parspawn.c
future_SOURCES       = future.c

allreduce_LDADD      = libexamples.la $(top_builddir)/libhpx/libhpx.la
fibonaccihpx_LDADD   = libexamples.la $(top_builddir)/libhpx/libhpx.la
pingponghpx_LDADD    = libexamples.la $(top_builddir)/libhpx/libhpx.la
countdown_LDADD      = libexamples.la $(top_builddir)/libhpx/libhpx.la
seqspawn_LDADD       = libexamples.la $(top_builddir)/libhpx/libhpx.la
parspawn_LDADD       = libexamples.la $(top_builddir)/libhpx/libhpx.la
future_LDADD         = libexamples.la $(top_builddir)/libhpx/libhpx.la

AM_CPPFLAGS          = -I$(top_srcdir)/include
AM_CFLAGS            = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
AM_CFLAGS           += @PHOTON_CFLAGS@ @MPI_CFLAGS@ @CRYPTO_CFLAGS@
AM_CFLAGS           += @HWLOC_CFLAGS@ @PAPI_CFLAGS@ @TCMALLOC_CFLAGS@
AM_CFLAGS           += @GLIB_CFLAGS@

AM_LDFLAGS          = -static

# allreduce uses MPI directly
allreduce_LDADD    += @MPI_LIBS@
