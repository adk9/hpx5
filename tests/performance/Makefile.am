noinst_bin_PROGRAMS     = perftest
noinst_bindir           = $(abs_top_builddir)/tests/performance
check_SCRIPTS           = $(abs_top_builddir)/tests/performance/run_tests.sh
TESTS                   = $(abs_top_builddir)/tests/performance/run_tests.sh
check_PROGRAMS		= perftest
perftest_SOURCES        = perftest.c \
                          perfinit.c \
                          primesieve.c  

perftest_LDADD           = $(top_builddir)/libhpx/libhpx.la -lm -lcheck  
perftest_CPPFLAGS        = -I$(top_srcdir)/include 
perftest_CPPFLAGS       += -D_POSIX_C_SOURCE=200809L

perftest_CFLAGS          = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
perftest_CFLAGS         += @CHECK_LIBS@ @PHOTON_CFLAGS@ @MPI_CFLAGS@ 
perftest_CFLAGS         += @CHECK_CFLAGS@ @HWLOC_CFLAGS@ @PAPI_CFLAGS@
perftest_CFLAGS         += $(HPX_C99)

# Turn off some warnings for the tests
perftest_CFLAGS         += -Wno-unused

if HAVE_PHOTON
perftest_CPPFLAGS       += $(HPX_JEMALLOC_LDFLAGS)
perftest_CFLAGS         += @PHOTON_CFLAGS@
perftest_LDFLAGS         = $(HPX_JEMALLOC_LDFLAGS)
perftest_LDADD          += -lphoton $(HPX_JEMALLOC_LIBS)
endif
