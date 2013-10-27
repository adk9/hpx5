noinst_bin_PROGRAMS 	= hpxtest
noinst_bindir 			= $(abs_top_builddir)/tests
#TESTS 					= hpxtest
TESTS 					= 
check_PROGRAMS			= hpxtest
hpxtest_SOURCES			= 00_hpxtest.c \
                          01_init.c \
                          02_mem.c \
                          03_ctx.c \
                          04_thread1.c \
                          05_queue.c \
                          06_kthread.c \
                          07_mctx.c \
                          08_thread2.c \
                          09_config.c \
                          10_list.c \
                          11_map.c \
                          12_gate.c \
                          98_thread_perf1.c

if CPU_X86_64
    hpxtest_SOURCES    += arch/x86_64/99_hpxtest.S
endif

if HAVE_NETWORK
    hpxtest_SOURCES    += 12_parcelhandler.c
endif

hpxtest_LDADD           = $(top_builddir)/src/libhpx.la -lm

hpxtest_CPPFLAGS        = -I$(top_srcdir)/include -I$(top_srcdir)/src
hpxtest_CPPFLAGS       += -D_POSIX_C_SOURCE=200809L

hpxtest_CFLAGS          = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR)
hpxtest_CFLAGS         += @CHECK_LIBS@ @PHOTON_CFLAGS@ @MPI_CFLAGS@ 
hpxtest_CFLAGS         += @CRYPTO_CFLAGS@ @HWLOC_CFLAGS@ @PAPI_CFLAGS@
hpxtest_CFLAGS         += @TCMALLOC_CFLAGS@ @GLIB_CFLAGS@
hpxtest_CFLAGS         += -std=c99

# Turn off some warnings for the tests
hpxtest_CFLAGS         += -Wno-unused
