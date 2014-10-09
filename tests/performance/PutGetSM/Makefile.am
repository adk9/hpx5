all: $(check_PROGRAMS)
#noinst_bindir           = $(builddir)
check_SCRIPTS           = $(srcdir)/run_tests.sh
TESTS                   = $(srcdir)/run_tests.sh

check_PROGRAMS          = $(PERFTESTS)
PERFTESTS               =

if ENABLE_LENGTHY_TESTS
PERFTESTS += \
        perfmain \
        guppie
else
check_PROGRAMS += \
        perfmain \
        guppie
endif

NPROCS ?= 2
LOG_COMPILER = $(TEST_RUNNER)

AM_LDFLAGS         = $(LIBTOOL_WRAPPER_LDFLAGS)
LDADD              = $(top_builddir)/libhpx/libhpx.la @CHECK_LIBS@ -lm
AM_CPPFLAGS        = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_CFLAGS          = @MPI_CFLAGS@ @CHECK_CFLAGS@
AM_CFLAGS         += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

perfmain_SOURCES   = perfmain.c
guppie_SOURCES     = guppie.c 
