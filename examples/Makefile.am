examplesdir          = $(datarootdir)/doc/@PACKAGE@

examples_PROGRAMS    = fibonacci pingpong allgather allreduce-bench allreduce \
                       countdown seqspawn parspawn future memput memget \
                       guppie sendrecv sendrecv-chan allreduce-chan \
                       pingpong-chan hello alltoall qsort call

fibonacci_SOURCES       = fibonacci.c
pingpong_SOURCES        = pingpong.c
allgather_SOURCES       = allgather.c
allreduce_bench_SOURCES = allreduce-bench.c
alltoall_SOURCES        = alltoall.c
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

all_LIBS = $(HPX_LDADD)

AM_CPPFLAGS          = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS            =

if HAVE_PHOTON
examples_PROGRAMS   += netfutures
netfutures_SOURCES   = netfutures.c
endif

if HAVE_MPI
AM_CFLAGS           += @MPI_CFLAGS@
endif

fibonacci_LDADD       = $(all_LIBS)
pingpong_LDADD        = $(all_LIBS)
allgather_LDADD       = $(all_LIBS)
allreduce_bench_LDADD = $(all_LIBS)
alltoall_LDADD        = $(all_LIBS)
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
