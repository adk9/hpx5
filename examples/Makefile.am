examplesdir          = $(datarootdir)/doc/@PACKAGE@

examples_PROGRAMS    = fibonacci pingpong allreduce countdown seqspawn \
                       parspawn future agas_test memput memget guppie sendrecv\
                       sendrecv-chan allreduce-chan pingpong-chan

fibonacci_SOURCES       = fibonacci.c
pingpong_SOURCES        = pingpong.c
allreduce_SOURCES       = allreduce.c
countdown_SOURCES       = countdown.c
seqspawn_SOURCES        = seqspawn.c
parspawn_SOURCES        = parspawn.c
future_SOURCES          = future.c
agas_test_SOURCES       = agas_test.c
memput_SOURCES          = memput.c
memget_SOURCES          = memget.c
guppie_SOURCES          = guppie.c
sendrecv_SOURCES        = sendrecv.c
sendrecv_chan_SOURCES   = sendrecv-chan.c
allreduce_chan_SOURCES  = allreduce-chan.c
pingpong_chan_SOURCES   = pingpong-chan.c

AM_CPPFLAGS          = -I$(top_srcdir)/include
AM_CFLAGS            = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
AM_LDFLAGS          = -static $(top_builddir)/libhpx/libhpx.la

if HAVE_HWLOC
AM_CFLAGS           += @HWLOC_CFLAGS@
endif

if HAVE_PAPI
AM_CFLAGS           += @PAPI_CFLAGS@
endif

if HAVE_PHOTON
AM_CFLAGS           += @PHOTON_CFLAGS@
AM_LDFLAGS          += -lphoton -ljemalloc
endif

if HAVE_MPI
AM_CFLAGS           += @MPI_CFLAGS@
endif
