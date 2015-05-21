examplesdir          = $(datarootdir)/doc/@PACKAGE@

noinst_PROGRAMS         = fibonacci pingpong allreduce countdown seqspawn \
                          parspawn future memput memget guppie sendrecv \
                          hello qsort call netbench 

fibonacci_SOURCES       = fibonacci.c
pingpong_SOURCES        = pingpong.c
allreduce_SOURCES       = allreduce.c
countdown_SOURCES       = countdown.c
seqspawn_SOURCES        = seqspawn.c
parspawn_SOURCES        = parspawn.c
future_SOURCES          = future.c
memput_SOURCES          = memput.c
memget_SOURCES          = memget.c
guppie_SOURCES          = guppie.c
sendrecv_SOURCES        = sendrecv.c
qsort_SOURCES           = qsort.c
hello_SOURCES           = hello.c
call_SOURCES            = call.c
netbench_SOURCES        = netbench.c

AM_CPPFLAGS          = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS            =
AM_LDFLAGS           = $(HPX_JEMALLOC_RPATH) -no-install
LDADD                = $(HPX_LIBS)

fibonacci_DEPENDENCIES       = $(HPX_DEPS)
pingpong_DEPENDENCIES        = $(HPX_DEPS)
allreduce_DEPENDENCIES       = $(HPX_DEPS)
countdown_DEPENDENCIES       = $(HPX_DEPS)
seqspawn_DEPENDENCIES        = $(HPX_DEPS)
parspawn_DEPENDENCIES        = $(HPX_DEPS)
future_DEPENDENCIES          = $(HPX_DEPS)
memput_DEPENDENCIES          = $(HPX_DEPS)
memget_DEPENDENCIES          = $(HPX_DEPS)
guppie_DEPENDENCIES          = $(HPX_DEPS)
sendrecv_DEPENDENCIES        = $(HPX_DEPS)
qsort_DEPENDENCIES           = $(HPX_DEPS)
hello_DEPENDENCIES           = $(HPX_DEPS)
call_DEPENDENCIES            = $(HPX_DEPS)
netbench_DEPENDENCIES        = $(HPX_DEPS)
