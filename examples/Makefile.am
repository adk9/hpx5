examplesdir          = $(datarootdir)/doc/@PACKAGE@

noinst_PROGRAMS         = fibonacci pingpong allreduce countdown seqspawn \
                          parspawn future memput memget guppie hello \
                          qsort call netbench 

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
qsort_SOURCES           = qsort.c
hello_SOURCES           = hello.c
call_SOURCES            = call.c
netbench_SOURCES        = netbench.c

AM_CPPFLAGS          = -D_GNU_SOURCE $(HPX_APPS_CFLAGS) -I$(top_srcdir)/include
AM_LDFLAGS           = $(LIBHPX_LDFLAGS) -no-install
LDADD                = $(HPX_APPS_LDADD)

fibonacci_DEPENDENCIES       = $(HPX_APPS_DEPS)
pingpong_DEPENDENCIES        = $(HPX_APPS_DEPS)
allreduce_DEPENDENCIES       = $(HPX_APPS_DEPS)
countdown_DEPENDENCIES       = $(HPX_APPS_DEPS)
seqspawn_DEPENDENCIES        = $(HPX_APPS_DEPS)
parspawn_DEPENDENCIES        = $(HPX_APPS_DEPS)
future_DEPENDENCIES          = $(HPX_APPS_DEPS)
memput_DEPENDENCIES          = $(HPX_APPS_DEPS)
memget_DEPENDENCIES          = $(HPX_APPS_DEPS)
guppie_DEPENDENCIES          = $(HPX_APPS_DEPS)
qsort_DEPENDENCIES           = $(HPX_APPS_DEPS)
hello_DEPENDENCIES           = $(HPX_APPS_DEPS)
call_DEPENDENCIES            = $(HPX_APPS_DEPS)
netbench_DEPENDENCIES        = $(HPX_APPS_DEPS)
