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
LDADD              = $(top_builddir)/libhpx/libhpx.la @CHECK_LIBS@ -lm -lcheck
AM_CPPFLAGS        = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_CFLAGS          = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
AM_CFLAGS         += @CHECK_LIBS@ @PHOTON_CFLAGS@ @MPI_CFLAGS@
AM_CFLAGS         += @CHECK_CFLAGS@ @HWLOC_CFLAGS@ @PAPI_CFLAGS@
AM_CFLAGS         += $(HPX_C99)

perfmain_SOURCES   = perfmain.c
guppie_SOURCES     = guppie.c 

if HAVE_PHOTON
LDADD            += -lphoton
endif
