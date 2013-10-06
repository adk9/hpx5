AM_CFLAGS = @CFLAGS@                         \
	    @MPI_CFLAGS@                     \
	    -I@top_srcdir@/include

bin_PROGRAMS = photon_test photon_test_get photon_test_put
AM_LDFLAGS = @top_srcdir@/src/libphoton.la -lm @THREAD_LIBS@ @UGNI_LIBS@ @VERBS_LIBS@ @MPI_LIBS@

photon_test_SOURCES = kernel.c photon_test.c
photon_test_get_SOURCES = photon_test_get.c
photon_test_put_SOURCES = photon_test_put.c
