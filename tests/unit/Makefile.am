noinst_HEADERS   = tests.h

noinst_PROGRAMS  = $(TESTS)

AM_CPPFLAGS      = -I$(top_srcdir)/include 
AM_CPPFLAGS     += -D_POSIX_C_SOURCE=200809L

# Turn off some warnings for the tests
AM_CFLAGS        = -Wno-unused

LDADD = $(HPX_LDADD)

TESTS            = test_and_lco           \
                   test_bcast             \
                   test_channels          \
                   test_futures           \
                   test_gencount          \
                   test_global_mem_alloc  \
                   test_lco               \
                   test_collectives       \
                   test_mem_alloc         \
                   test_memget            \
                   test_memput            \
                   test_parcel            \
                   test_call              \
                   test_sema              \
                   test_runtime           \
                   test_threads

if ENABLE_LENGTHY_TESTS
TESTS           += test_wait
endif

test_and_lco_SOURCES            = test_and_lco.c
test_bcast_SOURCES              = test_bcast.c
test_channels_SOURCES           = test_channels.c
test_futures_SOURCES            = test_futures.c
test_gencount_SOURCES           = test_gencount.c
test_global_mem_alloc_SOURCES   = test_global_mem_alloc.c
test_lco_SOURCES                = test_lco.c
test_collectives_SOURCES        = test_collectives.c
test_mem_alloc_SOURCES          = test_mem_alloc.c
test_memget_SOURCES             = test_memget.c
test_memput_SOURCES             = test_memput.c
test_parcel_SOURCES             = test_parcel.c
test_call_SOURCES               = test_call.c
test_sema_SOURCES               = test_sema.c
test_runtime_SOURCES            = test_runtime.c
test_threads_SOURCES            = test_threads.c

test_and_lco_DEPENDENCIES            = $(HPX_DEPS)
test_bcast_DEPENDENCIES              = $(HPX_DEPS)
test_channels_DEPENDENCIES           = $(HPX_DEPS)
test_futures_DEPENDENCIES            = $(HPX_DEPS)
test_gencount_DEPENDENCIES           = $(HPX_DEPS)
test_global_mem_alloc_DEPENDENCIES   = $(HPX_DEPS)
test_lco_DEPENDENCIES                = $(HPX_DEPS)
test_collectives_DEPENDENCIES        = $(HPX_DEPS)
test_mem_alloc_DEPENDENCIES          = $(HPX_DEPS)
test_memget_DEPENDENCIES             = $(HPX_DEPS)
test_memput_DEPENDENCIES             = $(HPX_DEPS)
test_parcel_DEPENDENCIES             = $(HPX_DEPS)
test_call_DEPENDENCIES               = $(HPX_DEPS)
test_sema_DEPENDENCIES               = $(HPX_DEPS)
test_runtime_DEPENDENCIES            = $(HPX_DEPS)
test_threads_DEPENDENCIES            = $(HPX_DEPS)

if ENABLE_LENGTHY_TESTS
test_wait_SOURCES                    = test_wait.c
endif

test_wait_DEPENDENCIES               = $(HPX_DEPS)

LOG_COMPILER = $(TESTS_CMD)
