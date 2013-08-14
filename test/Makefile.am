AM_CFLAGS = @CFLAGS@                         \
			@MPI_CFLAGS@                     \
			-I@top_srcdir@/include           \
			-I@top_srcdir@/libphoton/include

bin_PROGRAMS = photon_test
photon_test_SOURCES = kernel.c photon_test.c
photon_test_LDFLAGS = @top_srcdir@/libphoton/src/libphoton.la -lm @RDMA_LIBS@ @THREAD_LIBS@ @MPI_LIBS@
