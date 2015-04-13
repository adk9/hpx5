CollectiveBenchdir              = $(datarootdir)/doc/@PACKAGE@
CollectiveBench_PROGRAMS        = hpx_allreduce hpx_allgather hpx_alltoall

hpx_allreduce_SOURCES           = hpx_allreduce.c common.c
hpx_allgather_SOURCES           = hpx_allgather.c common.c
hpx_alltoall_SOURCES            = hpx_alltoall.c common.c

all_LIBS                    = $(HPX_LIBS)

AM_CPPFLAGS                 = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                   =
AM_LDFLAGS                  = $(HPX_JEMALLOC_RPATH) -no-install

if HAVE_MPI
AM_CFLAGS                      += @MPI_CFLAGS@
endif

hpx_allreduce_LDADD             = $(all_LIBS)
hpx_allgather_LDADD             = $(all_LIBS)
hpx_alltoall_LDADD              = $(all_LIBS)

hpx_allreduce_DEPENDENCIES      = $(HPX_DEPS)
hpx_allgather_DEPENDENCIES      = $(HPX_DEPS)
hpx_alltoall_DEPENDENCIES       = $(HPX_DEPS)
