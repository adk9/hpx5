examplesdir          = $(datarootdir)/doc/@PACKAGE@

noinst_PROGRAMS         = fibonacci pingpong allreduce countdown seqspawn \
                          parspawn future memput memget guppie sendrecv \
                          sendrecv-chan allreduce-chan pingpong-chan hello \
                          qsort call netbench netfutures

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
sendrecv_chan_SOURCES   = sendrecv-chan.c
allreduce_chan_SOURCES  = allreduce-chan.c
pingpong_chan_SOURCES   = pingpong-chan.c
qsort_SOURCES           = qsort.c
hello_SOURCES           = hello.c
call_SOURCES            = call.c
netbench_SOURCES        = netbench.c
netfutures_SOURCES      = netfutures.c

all_LIBS = $(HPX_JEMALLOC_LIBS) $(HPX_LIBS)

AM_CPPFLAGS          = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS            =
AM_LDFLAGS           = $(HPX_JEMALLOC_LDFLAGS) -no-install

fibonacci_LDADD       = $(all_LIBS)
pingpong_LDADD        = $(all_LIBS)
allreduce_LDADD       = $(all_LIBS)
countdown_LDADD       = $(all_LIBS)
seqspawn_LDADD        = $(all_LIBS)
parspawn_LDADD        = $(all_LIBS)
future_LDADD          = $(all_LIBS)
memput_LDADD          = $(all_LIBS)
memget_LDADD          = $(all_LIBS)
guppie_LDADD          = $(all_LIBS)
netfutures_LDADD      = $(all_LIBS)
sendrecv_LDADD        = $(all_LIBS)
sendrecv_chan_LDADD   = $(all_LIBS)
allreduce_chan_LDADD  = $(all_LIBS)
pingpong_chan_LDADD   = $(all_LIBS)
qsort_LDADD           = $(all_LIBS)
hello_LDADD           = $(all_LIBS)
call_LDADD            = $(all_LIBS)
netbench_LDADD        = $(all_LIBS)

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
netfutures_DEPENDENCIES      = $(HPX_DEPS)
sendrecv_DEPENDENCIES        = $(HPX_DEPS)
sendrecv_chan_DEPENDENCIES   = $(HPX_DEPS)
allreduce_chan_DEPENDENCIES  = $(HPX_DEPS)
pingpong_chan_DEPENDENCIES   = $(HPX_DEPS)
qsort_DEPENDENCIES           = $(HPX_DEPS)
hello_DEPENDENCIES           = $(HPX_DEPS)
call_DEPENDENCIES            = $(HPX_DEPS)
netbench_DEPENDENCIES        = $(HPX_DEPS)
