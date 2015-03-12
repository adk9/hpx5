AM_CFLAGS = @CFLAGS@                         \
	    @MPI_CFLAGS@                     \
	    -I@top_srcdir@/include

bin_PROGRAMS = photon_test             \
	       photon_test_get         \
	       photon_test_put         \
	       photon_test_put_wc      \
	       photon_threaded_put_wc  \
	       photon_test_forwarder   \
	       photon_test_interleaved \
	       photon_pingpong         \
	       photon_ud_blockid       \
	       photon_ud_pingpong      \
	       photon_ud_mton

AM_LDFLAGS = $(top_builddir)/src/libphoton.la -lm -lcrypto @THREAD_LIBS@ @UGNI_LIBS@ @VERBS_LIBS@ @MPI_LIBS@ -lrt -no-install

photon_test_SOURCES = kernel.c photon_test.c
photon_test_get_SOURCES = photon_test_get.c
photon_test_put_SOURCES = photon_test_put.c
photon_test_put_wc_SOURCES = photon_test_put_wc.c
photon_threaded_put_wc_SOURCES = photon_threaded_put_wc.c
photon_test_forwarder_SOURCES = photon_test_forwarder.c
photon_test_interleaved_SOURCES = photon_test_interleaved.c
photon_pingpong_SOURCES = photon_pingpong.c

photon_ud_blockid_SOURCES = ud_bravo_tests/photon_ud_blockid.c
photon_ud_pingpong_SOURCES = ud_bravo_tests/photon_ud_pingpong.c
photon_ud_mton_SOURCES = ud_bravo_tests/photon_ud_mton.c

photon_threaded_put_wc_CFLAGS = ${AM_CFLAGS} -D_GNU_SOURCE
