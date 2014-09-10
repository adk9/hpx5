noinst_bin_PROGRAMS = photontest
noinst_bindir       = $(abs_top_builddir)/tests
TESTS               = photontest
check_PROGRAMS      = photontest
photontest_SOURCES  = photonmain.c \
                      photoninit.c \
                      kernel.c \
                      photon_test.c 

photontest_LDADD    = $(top_builddir)/src/libphoton.la -lm -lcrypto -lcheck @CHECK_LIBS@ @THREAD_LIBS@ @UGNI_LIBS@ @VERBS_LIBS@ @MPI_LIBS@ -lrt
photontest_CFLAGS   = @CFLAGS@ @CHECK_CFLAGS@ @MPI_CFLAGS@ -I$(top_srcdir)/include
