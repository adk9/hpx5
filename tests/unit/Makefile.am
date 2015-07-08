noinst_HEADERS   = tests.h

noinst_PROGRAMS  = $(TESTS)

AM_CPPFLAGS      = $(LIBFFI_CFLAGS) -I$(top_srcdir)/include -Wno-unused
AM_LDFLAGS       = $(LIBHPX_LDFLAGS) -no-install
LDADD            = $(LIBHPX_LDADD)

TESTS = bcast                   \
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
        task                    \
        thread_cont_action      \
        thread_continue         \
        thread_create           \
        thread_gettlsid         \
        thread_yield

if ENABLE_LENGTHY_TESTS
TESTS           += lco_wait
endif

bcast_SOURCES                       =  bcast.c
call_when_SOURCES                   =  call_when.c
gas_alloc_SOURCES                   =  gas_alloc.c
gas_global_alloc_SOURCES            =  gas_global_alloc.c
gas_memget_SOURCES                  =  gas_memget.c
gas_memput_SOURCES                  =  gas_memput.c
lco_get_remote_SOURCES              =  lco_get_remote.c
lco_and_SOURCES                     =  lco_and.c
lco_array_SOURCES                   =  lco_array.c
lco_collectives_SOURCES             =  lco_collectives.c
lco_futures_SOURCES                 =  lco_futures.c
lco_gencount_SOURCES                =  lco_gencount.c
lco_sema_SOURCES                    =  lco_sema.c
lco_setget_SOURCES                  =  lco_setget.c
lco_user_SOURCES                    =  lco_user.c
libhpx_boot_SOURCES                 =  libhpx_boot.c
parcel_continuation_SOURCES         =  parcel_continuation.c
parcel_create_SOURCES               =  parcel_create.c
parcel_send_SOURCES                 =  parcel_send.c
parcel_send_rendezvous_SOURCES      =  parcel_send_rendezvous.c
parcel_send_through_SOURCES         =  parcel_send_through.c
process_SOURCES                     =  process.c
runtime_SOURCES                     =  runtime.c
task_SOURCES                        =  task.c
thread_cont_action_SOURCES          =  thread_cont_action.c
thread_continue_SOURCES             =  thread_continue.c
thread_create_SOURCES               =  thread_create.c
thread_gettlsid_SOURCES             =  thread_gettlsid.c
thread_yield_SOURCES                =  thread_yield.c

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
thread_yield_DEPENDENCIES           = $(LIBHPX_DEPS)


if ENABLE_LENGTHY_TESTS
lco_wait_SOURCES                    = lco_wait.c
endif

LOG_COMPILER = $(TESTS_CMD)
