noinst_PROGRAMS  = $(TESTS)

AM_CPPFLAGS      = -I$(top_srcdir)/include 
AM_CPPFLAGS     += -D_POSIX_C_SOURCE=200809L
AM_LDFLAGS       = $(HPX_JEMALLOC_AS_MALLOC_RPATH) $(HPX_JEMALLOC_RPATH) -lm

# Turn off some warnings for the tests
AM_CFLAGS        = -Wno-unused

LIBS = $(HPX_LIBS)

TESTS = perf_gas_alloc		\
        perf_gas_addr_trans	\
        perf_lco_chan		\
        perf_lco_sema		\
        perf_lco_future		\
        perf_lco_netfutures	\
        perf_lco_netfutures_msgSize


if ENABLE_LENGTHY_TESTS
TESTS += perf_lco_and
endif

LOG_COMPILER = $(TESTS_CMD)

perf_gas_alloc_SOURCES		= perf_gas_alloc.c
perf_gas_addr_trans_SOURCES	= perf_gas_addr_trans.c
perf_lco_chan_SOURCES		= perf_lco_chan.c
perf_lco_sema_SOURCES		= perf_lco_sema.c
perf_lco_future_SOURCES		= perf_lco_future.c

perf_gas_alloc_DEPENDENCIES			= $(HPX_DEPS)
perf_gas_addr_trans_DEPENDENCIES		= $(HPX_DEPS)
perf_lco_chan_DEPENDENCIES			= $(HPX_DEPS)
perf_lco_sema_DEPENDENCIES			= $(HPX_DEPS)
perf_lco_future_DEPENDENCIES			= $(HPX_DEPS)
perf_lco_netfutures_SOURCES			= perf_lco_netfutures.c
perf_lco_netfutures_DEPENDENCIES		= $(HPX_DEPS)
perf_lco_netfutures_msgSize_SOURCES		= perf_lco_netfutures_msgSize.c
perf_lco_netfutures_msgSize_DEPENDENCIES	= $(HPX_DEPS)

if ENABLE_LENGTHY_TESTS
perf_lco_and_SOURCES		= perf_lco_and.c
perf_lco_and_DEPENDENCIES	= $(HPX_DEPS)
endif



