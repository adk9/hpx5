benchdir          = $(datarootdir)/doc/@PACKAGE@

bench_PROGRAMS    = netbench

netbench_SOURCES  = netbench.c

CC = tau_cc.sh

AM_CPPFLAGS        = -I/u/jayaajay/tau-2.23.2b3/include  -I$(top_srcdir)/include
AM_CFLAGS          =
AM_LDFLAGS         = -static $(top_builddir)/libhpx/libhpx.la

if HAVE_PAPI
AM_CFLAGS           += @PAPI_CFLAGS@
endif

if HAVE_MPI
AM_CFLAGS           += @MPI_CFLAGS@
endif
