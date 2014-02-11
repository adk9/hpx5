AM_CFLAGS = @CFLAGS@                         \
	    @MPI_CFLAGS@                     \
	    -I@top_srcdir@/include

bin_PROGRAMS = photon_test photon_test_get photon_test_put photon_test_forwarder photon_test_interleaved photon_pingpong
AM_LDFLAGS = @top_srcdir@/src/libphoton.la -lm @THREAD_LIBS@ @UGNI_LIBS@ @VERBS_LIBS@ @MPI_LIBS@

photon_test_SOURCES = kernel.c photon_test.c
photon_test_get_SOURCES = photon_test_get.c
photon_test_put_SOURCES = photon_test_put.c
photon_test_forwarder_SOURCES = photon_test_forwarder.c
photon_test_interleaved_SOURCES = photon_test_interleaved.c
photon_pingpong_SOURCES = photon_pingpong.c