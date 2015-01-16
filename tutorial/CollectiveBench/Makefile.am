CollectiveBenchdir              = $(datarootdir)/doc/@PACKAGE@
CollectiveBench_PROGRAMS        = hpx_allreduce hpx_allgather hpx_alltoall

hpx_allreduce_SOURCES           = hpx_allreduce.c 
hpx_allgather_SOURCES           = hpx_allgather.c
hpx_alltoall_SOURCES            = hpx_alltoall.c

all_LIBS                        = $(HPX_LDADD)

AM_CPPFLAGS                     = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                       =

if HAVE_MPI
AM_CFLAGS                      += @MPI_CFLAGS@
endif

hpx_allreduce_LDADD             = $(all_LIBS)
hpx_allgather_LDADD             = $(all_LIBS)
hpx_alltoall_LDADD              = $(all_LIBS)
