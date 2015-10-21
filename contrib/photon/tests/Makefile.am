noinst_PROGRAMS     = photontest
check_SCRIPTS       = run_tests.sh
TESTS               = run_tests.sh
check_PROGRAMS      = photontest
photontest_SOURCES  = test_cfg.h                    \
		      tests.h                       \
		      cutter_ids.h                  \
		      photonmain.c                  \
                      photoninit.c                  \
                      kernel.c                      \
                      photon_test.c                 \
                      photon_data_movement.c        \
                      photon_message_passing.c      \
                      photon_pingpong.c             \
                      photon_buffers_remote.c       \
                      photon_buffers_private.c      \
                      photon_rendezvous.c           \
                      photon_one_sided_put.c        \
                      photon_one_sided_get.c        \
                      photon_rdma_with_completion.c \
                      photon_test_put_wc.c          \
                      photon_os_get_bench.c         \
                      photon_os_put_bench.c         \
                      photon_send_buffer_bench.c    \
                      photon_recv_buffer_bench.c    \
                      photon_send_buffer_bd_bench.c \
                      photon_recv_buffer_bd_bench.c \
                      photon_put_wc_bw.c            \
                      photon_threaded_put_wc.c 

AM_CPPFLAGS         = -D_GNU_SOURCE $(PHOTON_APPS_CPPFLAGS) -I$(top_srcdir)/include
AM_CFLAGS           = $(PHOTON_APPS_CFLAGS) @CHECK_CFLAGS@ -Wno-unused
AM_LDFLAGS          = $(PHOTON_APPS_LDFLAGS) @CHECK_LIBS@ -no-install -lcrypto
LDADD               = $(PHOTON_APPS_LDADD)

EXTRA_DIST          = run_tests.sh
