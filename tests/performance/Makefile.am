SUBDIRS = TaskSpawningSO MicroBench Overheads PutGetSM

TaskSpawningSOtests: $(MAKE) -C TaskSpawningSO buildtests

MicroBenchtests: $(MAKE) -C MicroBench buildtests

Overheadstests: $(MAKE) -C Overheads buildtests

PutGetSMtests: $(MAKE) -C PutGetSM buildtests

tests: TaskSpawningSOtests MicroBenchtests Overheadsstests PutGetSMtests

buildtests: tests

buildall: buildtests
.PHONY: buildall tests buildtests TaskSpawningSOtests MicroBenchtests Overheadstests putGetSM

NPROCS ?= 2
LOG_COMPILER = $(TEST_RUNNER)

AM_LDFLAGS         = $(LIBTOOL_WRAPPER_LDFLAGS)
LDADD              = $(top_builddir)/libhpx/libhpx.la @CHECK_LIBS@ -lm -lcheck
AM_CPPFLAGS        = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_CFLAGS          = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
AM_CFLAGS         += @CHECK_LIBS@ @PHOTON_CFLAGS@ @MPI_CFLAGS@
AM_CFLAGS         += @CHECK_CFLAGS@ @HWLOC_CFLAGS@ @PAPI_CFLAGS@
AM_CFLAGS         += $(HPX_C99)

if HAVE_PHOTON
LDADD            += -lphoton
endif
