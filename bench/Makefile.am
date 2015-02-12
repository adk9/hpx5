benchdir          = $(datarootdir)/doc/@PACKAGE@

bench_PROGRAMS    = netbench

netbench_SOURCES  = netbench.c
netbench_LDADD    = $(HPX_LDADD)

AM_CPPFLAGS        = -I$(top_srcdir)/include
AM_CFLAGS          =

if HAVE_PAPI
AM_CFLAGS           += @PAPI_CFLAGS@
endif

if HAVE_MPI
AM_CFLAGS           += @MPI_CFLAGS@
endif
