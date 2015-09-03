collectivesdir                = $(datarootdir)/doc/@PACKAGE@
collectives_PROGRAMS          = allreduce allgather alltoall 

AM_CPPFLAGS			= -D_GNU_SOURCE $(HPX_APPS_CFLAGS) -I$(top_srcdir)/include
AM_LDFLAGS			= $(LIBHPX_LDFLAGS) -no-install
LDADD				= $(HPX_APPS_LDADD)

allreduce_SOURCES             = allreduce.c 
allgather_SOURCES             = allgather.c
alltoall_SOURCES              = alltoall.c

allreduce_DEPENDENCIES        = $(HPX_APPS_DEPS)
allgather_DEPENDENCIES        = $(HPX_APPS_DEPS)
alltoall_DEPENDENCIES         = $(HPX_APPS_DEPS)
