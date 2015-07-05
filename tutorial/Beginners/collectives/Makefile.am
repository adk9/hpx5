collectivesdir                = $(datarootdir)/doc/@PACKAGE@
collectives_PROGRAMS          = allreduce allgather alltoall 

AM_CPPFLAGS                 = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_LDFLAGS                  = $(LIBHPX_LDFLAGS) -no-install
LDADD						= $(LIBHPX_LDADD)

allreduce_SOURCES             = allreduce.c 
allgather_SOURCES             = allgather.c
alltoall_SOURCES              = alltoall.c

allreduce_DEPENDENCIES        = $(LIBHPX_DEPS)
allgather_DEPENDENCIES        = $(LIBHPX_DEPS)
alltoall_DEPENDENCIES         = $(LIBHPX_DEPS)
