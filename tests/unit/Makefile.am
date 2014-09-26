noinst_bin_PROGRAMS 	= hpxtest
noinst_bindir           = $(abs_top_builddir)/tests/unit
#TESTS 			= hpxtest 
check_SCRIPTS           = $(abs_top_builddir)/tests/unit/run_tests.sh
TESTS                   = $(abs_top_builddir)/tests/unit/run_tests.sh
check_PROGRAMS		= hpxtest
hpxtest_SOURCES		= 00_hpxtest.c \
                          01_TestRuntime.c \
                          02_TestMemAlloc.c \
                          03_TestGlobalMemAlloc.c \
                          04_TestParcel.c \
                          05_TestThreads.c \
                          06_TestFutures.c \
                          07_TestLCO.c \
                          08_TestSema.c \
                          09_TestChannels.c \
                          10_TestAndLCO.c \
                          11_TestGenCount.c \
                          12_TestMemget.c \
                          13_TestMemput.c \
                          14_TestNewFutures.c	

hpxtest_LDADD           = $(top_builddir)/libhpx/libhpx.la @CHECK_LIBS@ -lm -lcheck  
hpxtest_CPPFLAGS        = -I$(top_srcdir)/include 
hpxtest_CPPFLAGS       += -D_POSIX_C_SOURCE=200809L

hpxtest_CFLAGS          = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
hpxtest_CFLAGS         += @CHECK_LIBS@ @PHOTON_CFLAGS@ @MPI_CFLAGS@ 
hpxtest_CFLAGS         += @CHECK_CFLAGS@ @HWLOC_CFLAGS@ @PAPI_CFLAGS@
hpxtest_CFLAGS         += $(HPX_C99)

# Turn off some warnings for the tests
hpxtest_CFLAGS         += -Wno-unused

if HAVE_PHOTON
hpxtest_LDADD          += -lphoton
endif
