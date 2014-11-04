noinst_PROGRAMS 	= hpxtest
check_SCRIPTS           = $(srcdir)/run_tests.sh
TESTS                   = $(srcdir)/run_tests.sh
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
                          13_TestMemput.c #\
			  #14_TestNewFutures.c

hpxtest_LDADD           = $(top_builddir)/libhpx/libhpx.la @CHECK_LIBS@ -lm
hpxtest_CPPFLAGS        = -I$(top_srcdir)/include 
hpxtest_CPPFLAGS       += -D_POSIX_C_SOURCE=200809L

hpxtest_CFLAGS          = @MPI_CFLAGS@ @CHECK_CFLAGS@
hpxtest_CFLAGS         += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

# Turn off some warnings for the tests
hpxtest_CFLAGS         += -Wno-unused
