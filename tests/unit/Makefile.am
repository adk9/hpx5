noinst_HEADERS   = tests.h

noinst_PROGRAMS  = $(TESTS)

AM_CPPFLAGS      = -I$(top_srcdir)/include 
AM_CPPFLAGS     += -D_POSIX_C_SOURCE=200809L

# Turn off some warnings for the tests
AM_CFLAGS        = -Wno-unused

LDADD = $(HPX_LDADD)
DEPENDENCIES = $(HPX_LDADD)

TESTS            = test_and_lco           \
                   test_channels          \
                   test_futures           \
                   test_gencount          \
                   test_global_mem_alloc  \
                   test_lco               \
                   test_mem_alloc         \
                   test_memget            \
                   test_memput            \
                   test_parcel            \
                   test_sema              \
                   test_threads           \
                   test_wait

test_and_lco_SOURCES            = test_and_lco.c
test_channels_SOURCES           = test_channels.c
test_futures_SOURCES            = test_futures.c
test_gencount_SOURCES           = test_gencount.c
test_global_mem_alloc_SOURCES   = test_global_mem_alloc.c
test_lco_SOURCES                = test_lco.c
test_mem_alloc_SOURCES          = test_mem_alloc.c
test_memget_SOURCES             = test_memget.c
test_memput_SOURCES             = test_memput.c
test_parcel_SOURCES             = test_parcel.c
test_sema_SOURCES               = test_sema.c
test_threads_SOURCES            = test_threads.c
test_wait_SOURCES               = test_wait.c

#if HAVE_PHOTON
#TESTS += test_netfutures
#test_netfutures_SOURCES         = test_netfutures.c
#endif

LOG_COMPILER = $(TESTS_CMD)
