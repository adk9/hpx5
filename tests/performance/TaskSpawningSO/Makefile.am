all: $(check_PROGRAMS)
noinst_bindir           = $(abs_top_builddir)/tests/performance/TaskSpawningSO
check_SCRIPTS           = $(abs_top_builddir)/tests/performance/TaskSpawningSO/run_tests.sh
TESTS                   = $(abs_top_builddir)/tests/performance/TaskSpawningSO/run_tests.sh

check_PROGRAMS          = $(PERFTESTS)
PERFTESTS                   =

if ENABLE_LENGTHY_TESTS
PERFTESTS += \
        perfmain \
        parspawn \
        seqspawn \
        fibonacci
else
check_PROGRAMS += \
        perfmain \
        parspawn \
        seqspawn \
        fibonacci
endif

NPROCS ?= 2
LOG_COMPILER = $(TEST_RUNNER)

AM_LDFLAGS         = $(LIBTOOL_WRAPPER_LDFLAGS)
LDADD              = $(top_builddir)/libhpx/libhpx.la @CHECK_LIBS@ -lm -lcheck
AM_CPPFLAGS        = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_CFLAGS          = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
AM_CFLAGS         += @CHECK_LIBS@ @PHOTON_CFLAGS@ @MPI_CFLAGS@
AM_CFLAGS         += @CHECK_CFLAGS@ @HWLOC_CFLAGS@ @PAPI_CFLAGS@
AM_CFLAGS         += $(HPX_C99)

perfmain_SOURCES     = perfmain.c
parspawn_SOURCES     = parspawn.c 
seqspawn_SOURCES     = seqspawn.c
fibonacci_SOURCES    = fibonacci.c

if HAVE_PHOTON
LDADD            += -lphoton
endif
