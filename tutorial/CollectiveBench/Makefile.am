CollectiveBenchdir              = $(datarootdir)/doc/@PACKAGE@
CollectiveBench_PROGRAMS        = hpx_allreduce hpx_allgather hpx_alltoall

AM_CPPFLAGS			= -D_GNU_SOURCE $(HPX_APPS_CFLAGS) -I$(top_srcdir)/include
AM_LDFLAGS			= $(LIBHPX_LDFLAGS) -no-install
LDADD				= $(HPX_APPS_LDADD)

hpx_allreduce_SOURCES           = hpx_allreduce.c common.c
hpx_allgather_SOURCES           = hpx_allgather.c common.c
hpx_alltoall_SOURCES            = hpx_alltoall.c common.c

hpx_allreduce_DEPENDENCIES      = $(HPX_APPS_DEPS)
hpx_allgather_DEPENDENCIES      = $(HPX_APPS_DEPS)
hpx_alltoall_DEPENDENCIES       = $(HPX_APPS_DEPS)
