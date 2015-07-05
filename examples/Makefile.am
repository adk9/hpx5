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
AM_LDFLAGS           = $(LIBHPX_LDFLAGS) -no-install
LDADD                = $(LIBHPX_LDADD)

fibonacci_DEPENDENCIES       = $(LIBHPX_DEPS)
pingpong_DEPENDENCIES        = $(LIBHPX_DEPS)
allreduce_DEPENDENCIES       = $(LIBHPX_DEPS)
countdown_DEPENDENCIES       = $(LIBHPX_DEPS)
seqspawn_DEPENDENCIES        = $(LIBHPX_DEPS)
parspawn_DEPENDENCIES        = $(LIBHPX_DEPS)
future_DEPENDENCIES          = $(LIBHPX_DEPS)
memput_DEPENDENCIES          = $(LIBHPX_DEPS)
memget_DEPENDENCIES          = $(LIBHPX_DEPS)
guppie_DEPENDENCIES          = $(LIBHPX_DEPS)
sendrecv_DEPENDENCIES        = $(LIBHPX_DEPS)
qsort_DEPENDENCIES           = $(LIBHPX_DEPS)
hello_DEPENDENCIES           = $(LIBHPX_DEPS)
call_DEPENDENCIES            = $(LIBHPX_DEPS)
netbench_DEPENDENCIES        = $(LIBHPX_DEPS)
