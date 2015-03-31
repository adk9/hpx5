noinst_PROGRAMS = photontest
check_SCRIPTS       = run_tests.sh
TESTS               = run_tests.sh
check_PROGRAMS      = photontest
photontest_SOURCES  = photonmain.c \
                      photoninit.c \
                      kernel.c \
                      photon_test.c \
                      photon_data_movement.c \
                      photon_message_passing.c \
                      photon_pingpong.c \
                      photon_buffers_remote.c \
                      photon_buffers_private.c \
                      photon_rendezvous.c \
                      photon_one_sided_put.c \
                      photon_one_sided_get.c \
                      photon_rdma_with_completion.c \
                      photon_test_put_wc.c \
                      photon_os_get_bench.c \
                      photon_os_put_bench.c \
                      photon_send_buffer_bench.c \
                      photon_recv_buffer_bench.c \
                      photon_send_buffer_bd_bench.c \
                      photon_recv_buffer_bd_bench.c \
                      photon_put_wc_bw.c \
                      photon_threaded_put_wc.c 

photontest_LDADD    = $(top_builddir)/src/libphoton.la
photontest_LDFLAGS  = -lm -lcrypto -lcheck @CHECK_LIBS@ @THREAD_LIBS@ @UGNI_LIBS@ @VERBS_LIBS@ @MPI_LIBS@ -lrt -no-install
photontest_CFLAGS   = @CFLAGS@ @CHECK_CFLAGS@ @MPI_CFLAGS@ -I$(top_srcdir)/include -D_GNU_SOURCE
photontest_CFLAGS  += -g -Wunused -Wno-unused-result -Wunused-function -Wunused-label -Wunused-parameter -Wunused-value -Wunused-variable
