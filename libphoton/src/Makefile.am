AM_CFLAGS = @CFLAGS@                         \
			@MPI_CFLAGS@                     \
			-I@top_srcdir@/include           \
			-I@top_srcdir@/libphoton/include \
			-I@top_srcdir@/libphoton/util

lib_LTLIBRARIES = libphoton.la
libphoton_la_SOURCES = verbs.c                     \
						verbs_buffer.c             \
						verbs_remote_buffer.c      \
						verbs_rdma_info_ledger.c   \
						verbs_rdma_FIN_ledger.c 

libphoton_ladir = @top_srcdir@/libphoton/include
libphoton_la_HEADERS = *.h
libphoton_la_LDFLAGS = @top_srcdir@/libphoton/util/libutil.la
