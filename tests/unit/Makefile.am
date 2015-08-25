noinst_HEADERS   = tests.h

noinst_PROGRAMS  = $(TESTS)

AM_CPPFLAGS      = $(LIBFFI_CFLAGS) -I$(top_srcdir)/include -Wno-unused $(LIBHPX_CFLAGS)
AM_LDFLAGS       = $(LIBHPX_LDFLAGS) -no-install
LDADD            = $(LIBHPX_LDADD)

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

bcast_DEPENDENCIES                  = $(LIBHPX_DEPS)
call_when_DEPENDENCIES              = $(LIBHPX_DEPS)
gas_alloc_DEPENDENCIES              = $(LIBHPX_DEPS)
gas_global_alloc_DEPENDENCIES       = $(LIBHPX_DEPS)
gas_memget_DEPENDENCIES             = $(LIBHPX_DEPS)
gas_memput_DEPENDENCIES             = $(LIBHPX_DEPS)
lco_and_DEPENDENCIES                = $(LIBHPX_DEPS)
lco_array_DEPENDENCIES              = $(LIBHPX_DEPS)
lco_collectives_DEPENDENCIES        = $(LIBHPX_DEPS)
lco_futures_DEPENDENCIES            = $(LIBHPX_DEPS)
lco_gencount_DEPENDENCIES           = $(LIBHPX_DEPS)
lco_get_remote_DEPENDENCIES         = $(LIBHPX_DEPS)
lco_reduce_DEPENDENCIES             = $(LIBHPX_DEPS)
lco_sema_DEPENDENCIES               = $(LIBHPX_DEPS)
lco_setget_DEPENDENCIES             = $(LIBHPX_DEPS)
lco_user_DEPENDENCIES               = $(LIBHPX_DEPS)
lco_wait_DEPENDENCIES               = $(LIBHPX_DEPS)
libhpx_boot_DEPENDENCIES            = $(LIBHPX_DEPS)
parcel_continuation_DEPENDENCIES    = $(LIBHPX_DEPS)
parcel_create_DEPENDENCIES          = $(LIBHPX_DEPS)
parcel_send_DEPENDENCIES            = $(LIBHPX_DEPS)
parcel_send_rendezvous_DEPENDENCIES = $(LIBHPX_DEPS)
parcel_send_through_DEPENDENCIES    = $(LIBHPX_DEPS)
process_DEPENDENCIES                = $(LIBHPX_DEPS)
runtime_DEPENDENCIES                = $(LIBHPX_DEPS)
task_DEPENDENCIES                   = $(LIBHPX_DEPS)
thread_cont_action_DEPENDENCIES     = $(LIBHPX_DEPS)
thread_continue_DEPENDENCIES        = $(LIBHPX_DEPS)
thread_create_DEPENDENCIES          = $(LIBHPX_DEPS)
thread_gettlsid_DEPENDENCIES        = $(LIBHPX_DEPS)
thread_set_affinity_DEPENDENCIES    = $(LIBHPX_DEPS)
thread_yield_DEPENDENCIES           = $(LIBHPX_DEPS)
