all: $(check_PROGRAMS)
#noinst_bindir           = $(builddir)
check_SCRIPTS           = $(srcdir)/run_tests.sh
TESTS                   = $(srcdir)/run_tests.sh

check_PROGRAMS          = $(PERFTESTS)
PERFTESTS               =

if ENABLE_LENGTHY_TESTS
PERFTESTS += \
        time_gas_alloc \
        time_gas_addr_trans \
        time_lco_chan \
        time_lco_sema \
        time_lco_and \
        time_lco_future \
        time_lco_allreduce \
        time_lco_allgather \
        time_lco_alltoall
else
check_PROGRAMS += \
        time_gas_alloc \
        time_gas_addr_trans \
        time_lco_chan \
        time_lco_sema \
        time_lco_and \
        time_lco_future \
        time_lco_allreduce \
        time_lco_allgather \
        time_lco_alltoall
endif

NPROCS ?= 2
LOG_COMPILER = $(TEST_RUNNER)

AM_LDFLAGS         = $(LIBTOOL_WRAPPER_LDFLAGS)
LDADD              = $(top_builddir)/libhpx/libhpx.la @CHECK_LIBS@ @MPI_LIBS@ -lm
AM_CPPFLAGS        = -I$(top_srcdir)/include -D_POSIX_C_SOURCE=200809L
AM_CFLAGS          = @MPI_CFLAGS@ @CHECK_CFLAGS@ @HWLOC_CFLAGS@

time_gas_alloc_SOURCES		= time_gas_alloc.c
time_gas_addr_trans_SOURCES	= time_gas_addr_trans.c
time_lco_chan_SOURCES		= time_lco_chan.c
time_lco_sema_SOURCES		= time_lco_sema.c
time_lco_and_SOURCES		= time_lco_and.c
time_lco_future_SOURCES		= time_lco_future.c
time_lco_allreduce_SOURCES	= time_lco_allreduce.c
time_lco_allgather_SOURCES	= time_lco_allgather.c
time_lco_alltoall_SOURCES	= time_lco_alltoall.c

if HAVE_PHOTON
if ENABLE_LENGTHY_TESTS
PERFTESTS           += time_lco_netfutures \
                       time_lco_netfutures_msgSize
else
check_PROGRAMS      += time_lco_netfutures \
                       time_lco_netfutures_msgSize
endif
time_lco_netfutures_SOURCES		= time_lco_netfutures.c
time_lco_netfutures_msgSize_SOURCES	= time_lco_netfutures_msgSize.c
endif

