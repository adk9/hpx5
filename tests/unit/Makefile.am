noinst_HEADERS   = tests.h

noinst_PROGRAMS  = $(TESTS)

AM_CPPFLAGS      = $(HPX_APPS_CPPFLAGS) $(HPX_APPS_CFLAGS) -I$(top_srcdir)/include -Wno-unused
AM_LDFLAGS       = -no-install
LDADD            = $(HPX_APPS_LDADD)

LOG_COMPILER = $(TESTS_CMD)

TESTS = apex_test               \
	    allreduce               \
        bcast                   \
        call_when               \
        gas_alloc               \
        gas_global_alloc        \
        gas_memget              \
        gas_memput              \
		lco_get_remote          \
        lco_and                 \
        lco_array               \
        lco_collectives         \
        lco_futures             \
        lco_gencount            \
        lco_reduce              \
        lco_sema                \
        lco_setget              \
        lco_user                \
        libhpx_boot             \
        parcel_continuation     \
        parcel_create           \
        parcel_send             \
        parcel_send_rendezvous  \
        parcel_send_through     \
        process                 \
        runtime                 \
        thread_set_affinity     \
        task                    \
        thread_cont_action      \
        thread_continue         \
        thread_create           \
        thread_gettlsid         \
        thread_yield

if ENABLE_LENGTHY_TESTS
TESTS           += lco_wait
endif

bcast_DEPENDENCIES                  = $(HPX_APPS_DEPS)
call_when_DEPENDENCIES              = $(HPX_APPS_DEPS)
gas_alloc_DEPENDENCIES              = $(HPX_APPS_DEPS)
gas_global_alloc_DEPENDENCIES       = $(HPX_APPS_DEPS)
gas_memget_DEPENDENCIES             = $(HPX_APPS_DEPS)
gas_memput_DEPENDENCIES             = $(HPX_APPS_DEPS)
lco_and_DEPENDENCIES                = $(HPX_APPS_DEPS)
lco_array_DEPENDENCIES              = $(HPX_APPS_DEPS)
lco_collectives_DEPENDENCIES        = $(HPX_APPS_DEPS)
lco_futures_DEPENDENCIES            = $(HPX_APPS_DEPS)
lco_gencount_DEPENDENCIES           = $(HPX_APPS_DEPS)
lco_get_remote_DEPENDENCIES         = $(HPX_APPS_DEPS)
lco_reduce_DEPENDENCIES             = $(HPX_APPS_DEPS)
lco_sema_DEPENDENCIES               = $(HPX_APPS_DEPS)
lco_setget_DEPENDENCIES             = $(HPX_APPS_DEPS)
lco_user_DEPENDENCIES               = $(HPX_APPS_DEPS)
lco_wait_DEPENDENCIES               = $(HPX_APPS_DEPS)
libhpx_boot_DEPENDENCIES            = $(HPX_APPS_DEPS)
parcel_continuation_DEPENDENCIES    = $(HPX_APPS_DEPS)
parcel_create_DEPENDENCIES          = $(HPX_APPS_DEPS)
parcel_send_DEPENDENCIES            = $(HPX_APPS_DEPS)
parcel_send_rendezvous_DEPENDENCIES = $(HPX_APPS_DEPS)
parcel_send_through_DEPENDENCIES    = $(HPX_APPS_DEPS)
process_DEPENDENCIES                = $(HPX_APPS_DEPS)
runtime_DEPENDENCIES                = $(HPX_APPS_DEPS)
task_DEPENDENCIES                   = $(HPX_APPS_DEPS)
thread_cont_action_DEPENDENCIES     = $(HPX_APPS_DEPS)
thread_continue_DEPENDENCIES        = $(HPX_APPS_DEPS)
thread_create_DEPENDENCIES          = $(HPX_APPS_DEPS)
thread_gettlsid_DEPENDENCIES        = $(HPX_APPS_DEPS)
thread_set_affinity_DEPENDENCIES    = $(HPX_APPS_DEPS)
thread_yield_DEPENDENCIES           = $(HPX_APPS_DEPS)
