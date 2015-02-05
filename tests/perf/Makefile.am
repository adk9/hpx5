all: $(check_PROGRAMS)

TESTS = \
        perf_gas_alloc \
        perf_gas_addr_trans \
        perf_lco_chan \
        perf_lco_sema \
        perf_lco_and \
        perf_lco_future \
        perf_lco_allreduce \
        perf_lco_allgather \
        perf_lco_alltoall

if ENABLE_LENGTHY_TESTS
TESTS += perf_lco_and
endif

LOG_COMPILER = $(TESTS_CMD)

AM_CPPFLAGS      = -I$(top_srcdir)/include 
AM_CPPFLAGS     += -D_POSIX_C_SOURCE=200809L

# Turn off some warnings for the tests
AM_CFLAGS       += -Wno-unused

LDADD = $(HPX_LDADD)
DEPENDENCIES = $(HPX_LDADD)

perf_gas_alloc_SOURCES		= perf_gas_alloc.c
perf_gas_addr_trans_SOURCES	= perf_gas_addr_trans.c
perf_lco_chan_SOURCES		= perf_lco_chan.c
perf_lco_sema_SOURCES		= perf_lco_sema.c
perf_lco_and_SOURCES		= perf_lco_and.c
perf_lco_future_SOURCES		= perf_lco_future.c
perf_lco_allreduce_SOURCES	= perf_lco_allreduce.c
perf_lco_allgather_SOURCES	= perf_lco_allgather.c
perf_lco_alltoall_SOURCES	= perf_lco_alltoall.c

perf_gas_alloc_DEPENDENCIES	= $(HPX_LDADD)
perf_gas_addr_trans_DEPENDENCIES= $(HPX_LDADD)
perf_lco_chan_DEPENDENCIES	= $(HPX_LDADD)
perf_lco_sema_DEPENDENCIES	= $(HPX_LDADD)
perf_lco_and_DEPENDENCIES	= $(HPX_LDADD)
perf_lco_future_DEPENDENCIES	= $(HPX_LDADD)
perf_lco_allreduce_DEPENDENCIES	= $(HPX_LDADD)
perf_lco_allgather_DEPENDENCIES	= $(HPX_LDADD)
perf_lco_alltoall_DEPENDENCIES	= $(HPX_LDADD)

if HAVE_PHOTON
TESTS           += time_lco_netfutures \
                   time_lco_netfutures_msgSize

perf_lco_netfutures_SOURCES		= perf_lco_netfutures.c
perf_lco_netfutures_DEPENDENCIES	= $(HPX_LDADD)
perf_lco_netfutures_msgSize_SOURCES	= perf_lco_netfutures_msgSize.c
perf_lco_netfutures_msgSize_DEPENDENCIES= $(HPX_LDADD)
endif

