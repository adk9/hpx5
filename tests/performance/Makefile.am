SUBDIRS = . TaskSpawningSO MicroBench Overheads PutGetSM

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
LDADD              = $(top_builddir)/libhpx/libhpx.la @CHECK_LIBS@ -lm
AM_CPPFLAGS        = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_CFLAGS          = @PHOTON_CFLAGS@ @MPI_CFLAGS@
AM_CFLAGS         += @CHECK_CFLAGS@ @HWLOC_CFLAGS@

if HAVE_PHOTON
LDADD            += @PHOTON_LIBS@
endif
