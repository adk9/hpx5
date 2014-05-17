examplesdir          = $(datarootdir)/doc/@PACKAGE@

examples_PROGRAMS    = fibonacci pingpong allreduce countdown seqspawn \
                       parspawn future agas_test memput memget guppie sendrecv\
                       sendrecv-chan allreduce-chan

fibonacci_SOURCES    = fibonacci.c
pingpong_SOURCES     = pingpong.c
allreduce_SOURCES    = allreduce.c
countdown_SOURCES    = countdown.c
seqspawn_SOURCES     = seqspawn.c
parspawn_SOURCES     = parspawn.c
future_SOURCES       = future.c
agas_test_SOURCES    = agas_test.c
memput_SOURCES       = memput.c
memget_SOURCES       = memget.c
guppie_SOURCES       = guppie.c
sendrecv_SOURCES     = sendrecv.c
sendrecv_chan_SOURCES = sendrecv-chan.c
allreduce_chan_SOURCES = allreduce-chan.c

allreduce_LDADD      = $(top_builddir)/libhpx/libhpx.la
fibonacci_LDADD      = $(top_builddir)/libhpx/libhpx.la
pingpong_LDADD    	 = $(top_builddir)/libhpx/libhpx.la
countdown_LDADD      = $(top_builddir)/libhpx/libhpx.la
seqspawn_LDADD       = $(top_builddir)/libhpx/libhpx.la
parspawn_LDADD       = $(top_builddir)/libhpx/libhpx.la
future_LDADD         = $(top_builddir)/libhpx/libhpx.la
agas_test_LDADD      = $(top_builddir)/libhpx/libhpx.la
memput_LDADD         = $(top_builddir)/libhpx/libhpx.la
memget_LDADD         = $(top_builddir)/libhpx/libhpx.la
guppie_LDADD         = $(top_builddir)/libhpx/libhpx.la
sendrecv_LDADD       = $(top_builddir)/libhpx/libhpx.la
sendrecv_chan_LDADD  = $(top_builddir)/libhpx/libhpx.la
allreduce_chan_LDADD = $(top_builddir)/libhpx/libhpx.la

AM_CPPFLAGS          = -I$(top_srcdir)/include
AM_CFLAGS            = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
AM_CFLAGS           += @PHOTON_CFLAGS@ @MPI_CFLAGS@ @CRYPTO_CFLAGS@
AM_CFLAGS           += @HWLOC_CFLAGS@ @PAPI_CFLAGS@ @GLIB_CFLAGS@

AM_LDFLAGS          = -static @TREMA_LIBS@ # FIXME
