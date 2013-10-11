examplesdir          = $(datarootdir)/doc/@PACKAGE@

examples_PROGRAMS    = fibonaccihpx pingponghpx allreduce
fibonaccihpx_SOURCES = fibonacci.c
pingponghpx_SOURCES  = pingpong.c
allreduce_SOURCES    = allreduce.c

if HAVE_NETWORK
    NETWORK_DEFS     = -DHAVE_NETWORK=1
endif

if HAVE_MPI
    MPI_DEFS         = -DHAVE_MPI=1
endif

if HAVE_PHOTON
    PHOTON_DEFS      = -DHAVE_PHOTON=1
endif

AM_CPPFLAGS          = -I$(top_srcdir)/include
AM_CPPFLAGS         += $(NETWORK_DEFS) $(MPI_DEFS) $(PHOTON_DEFS) 

allreduce_LDADD      = $(top_builddir)/src/libhpx.la
fibonaccihpx_LDADD   = $(top_builddir)/src/libhpx.la
pingponghpx_LDADD    = $(top_builddir)/src/libhpx.la

AM_CFLAGS            = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_F_LTO)
AM_CFLAGS           += @PHOTON_CFLAGS@ @MPI_CFLAGS@ @LIBCRYPTO_CFLAGS@
AM_CFLAGS           += @HWLOC_CFLAGS@ @PAPI_CFLAGS@ @TCMALLOC_CFLAGS@
AM_CFLAGS           += -std=c99
