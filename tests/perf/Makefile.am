noinst_PROGRAMS  = $(TESTS)

AM_CPPFLAGS      = -I$(top_srcdir)/include 
AM_LDFLAGS       = $(HPX_JEMALLOC_LDFLAGS) $(HPX_JEMALLOC_RPATH) -no-install

# Turn off some warnings for the tests
AM_CFLAGS        = -Wno-unused

LIBS = $(HPX_LIBS)

TESTS = gas_alloc	\
        gas_addr_trans	\
        lco_sema	\
        lco_future	\
        thread_switch

if ENABLE_LENGTHY_TESTS
TESTS += lco_and
endif

LOG_COMPILER = $(TESTS_CMD)

gas_alloc_SOURCES			= gas_alloc.c
gas_addr_trans_SOURCES			= gas_addr_trans.c
lco_sema_SOURCES			= lco_sema.c
lco_future_SOURCES			= lco_future.c
thread_switch_SOURCES			= thread_switch.c

gas_alloc_DEPENDENCIES			= $(HPX_DEPS)
gas_addr_trans_DEPENDENCIES		= $(HPX_DEPS)
lco_sema_DEPENDENCIES			= $(HPX_DEPS)
lco_future_DEPENDENCIES			= $(HPX_DEPS)
thread_switch_DEPENDENCIES		= $(HPX_DEPS)

if ENABLE_LENGTHY_TESTS
lco_and_SOURCES		= lco_and.c
lco_and_DEPENDENCIES	= $(HPX_DEPS)
endif
