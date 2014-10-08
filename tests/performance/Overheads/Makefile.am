all: $(check_PROGRAMS)
#noinst_bindir           = $(builddir)
check_SCRIPTS           = $(srcdir)/run_tests.sh
TESTS                   = $(srcdir)/run_tests.sh

check_PROGRAMS          = $(PERFTESTS)
PERFTESTS               =

if ENABLE_LENGTHY_TESTS
PERFTESTS += \
        perfmain \
        time_gas_alloc \
        time_gas_addr_trans \
        time_lco_chan \
        time_lco_sema \
        time_lco_and \
        time_lco_future \
        time_lco_allreduce \
        time_lco_allgather \
        time_lco_alltoall \
        time_lco_netfutures \
        time_lco_netfutures_msgSize
else
check_PROGRAMS += \
        perfmain \
        time_gas_alloc \
        time_gas_addr_trans \
        time_lco_chan \
        time_lco_sema \
        time_lco_and \
        time_lco_future \
        time_lco_allreduce \
        time_lco_allgather \
        time_lco_alltoall \
        time_lco_netfutures \
        time_lco_netfutures_msgSize
endif

NPROCS ?= 2
LOG_COMPILER = $(TEST_RUNNER)

AM_LDFLAGS         = $(LIBTOOL_WRAPPER_LDFLAGS)
LDADD              = $(top_builddir)/libhpx/libhpx.la @CHECK_LIBS@ -lm -lcheck
AM_CPPFLAGS        = -I$(top_srcdir)/include -I$(top_srcdir)/tests/performance/common -D_POSIX_C_SOURCE=200809L
AM_CFLAGS          = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
AM_CFLAGS         += @CHECK_LIBS@ @PHOTON_CFLAGS@ @MPI_CFLAGS@
AM_CFLAGS         += @CHECK_CFLAGS@ @HWLOC_CFLAGS@ @PAPI_CFLAGS@
AM_CFLAGS         += $(HPX_C99)

perfmain_SOURCES   = perfmain.c
time_gas_alloc_SOURCES     = time_gas_alloc.c
time_gas_addr_trans_SOURCES = time_gas_addr_trans.c
time_lco_chan_SOURCES = time_lco_chan.c
time_lco_sema_SOURCES = time_lco_sema.c
time_lco_and_SOURCES = time_lco_and.c
time_lco_future_SOURCES = time_lco_future.c
time_lco_allreduce_SOURCES = time_lco_allreduce.c
time_lco_allgather_SOURCES = time_lco_allgather.c
time_lco_alltoall_SOURCES = time_lco_alltoall.c
time_lco_netfutures_SOURCES = time_lco_netfutures.c
time_lco_netfutures_msgSize_SOURCES = time_lco_netfutures_msgSize.c

if HAVE_PHOTON
LDADD            += -lphoton
endif

