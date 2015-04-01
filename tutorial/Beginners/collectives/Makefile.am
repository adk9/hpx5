collectivesdir                = $(datarootdir)/doc/@PACKAGE@
collectives_PROGRAMS          = allreduce allgather alltoall 

allreduce_SOURCES             = allreduce.c 
allgather_SOURCES             = allgather.c
alltoall_SOURCES              = alltoall.c

all_LIBS                    = $(HPX_LIBS)

AM_CPPFLAGS                 = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                   =
AM_LDFLAGS                  = $(HPX_JEMALLOC_RPATH) -no-install

if HAVE_MPI
AM_CFLAGS                    += @MPI_CFLAGS@
endif

allreduce_LDADD               = $(all_LIBS)
allgather_LDADD               = $(all_LIBS)
alltoall_LDADD                = $(all_LIBS)

allreduce_DEPENDENCIES        = $(HPX_DEPS)
allgather_DEPENDENCIES        = $(HPX_DEPS)
alltoall_DEPENDENCIES         = $(HPX_DEPS)
