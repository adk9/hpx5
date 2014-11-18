SUBDIRS = . overheads

Overheadstests: $(MAKE) -C overheads buildtests

tests: overheads_tests

buildtests: tests

buildall: buildtests
.PHONY: buildall tests buildtests overheads_tests 

NPROCS ?= 2
LOG_COMPILER = $(TEST_RUNNER)

AM_LDFLAGS         = $(LIBTOOL_WRAPPER_LDFLAGS)
LDADD              = $(top_builddir)/libhpx/libhpx.la @CHECK_LIBS@ -lm
AM_CPPFLAGS        = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_CFLAGS          = @MPI_CFLAGS@ @CHECK_CFLAGS@ @HWLOC_CFLAGS@

