noinst_PROGRAMS  = $(TESTS)

AM_CPPFLAGS      = $(LIBFFI_CFLAGS) -I$(top_srcdir)/include -Wno-unused
AM_LDFLAGS       = $(LIBHPX_LDFLAGS) -no-install
LDADD            = $(LIBHPX_LDADD)

TESTS = gas_alloc	\
        gas_addr_trans	\
        lco_sema	\
        lco_future	\
        thread_switch

if ENABLE_LENGTHY_TESTS
TESTS += lco_and
endif

LOG_COMPILER = $(TESTS_CMD)

gas_alloc_SOURCES				= gas_alloc.c
gas_addr_trans_SOURCES			= gas_addr_trans.c
lco_sema_SOURCES				= lco_sema.c
lco_future_SOURCES				= lco_future.c
thread_switch_SOURCES			= thread_switch.c

gas_alloc_DEPENDENCIES			= $(LIBHPX_DEPS)
gas_addr_trans_DEPENDENCIES		= $(LIBHPX_DEPS)
lco_sema_DEPENDENCIES			= $(LIBHPX_DEPS)
lco_future_DEPENDENCIES			= $(LIBHPX_DEPS)
thread_switch_DEPENDENCIES		= $(LIBHPX_DEPS)

if ENABLE_LENGTHY_TESTS
lco_and_SOURCES					= lco_and.c
lco_and_DEPENDENCIES			= $(LIBHPX_DEPS)
endif
