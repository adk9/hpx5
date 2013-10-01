if WITH_TESTSUITE

noinst_bindir=.
noinst_bin_PROGRAMS = hpxtest
#TESTS = hpxtest
TESTS = 
check_PROGRAMS = hpxtest
hpxtest_SOURCES = 00_hpxtest.c
hpxtest_CFLAGS = @CHECK_CFLAGS@ @PAPI_CFLAGS@
hpxtest_LDADD = $(top_builddir)/src/libhpx.la -lm -lrt -lpthread @CHECK_LIBS@ @PAPI_LIBS@

if CPU_X86_64
  ARCH_SRC = arch/x86_64/99_hpxtest.S
endif

hpxtest_SOURCES += $(ARCH_SRC)

if HAVE_MPI
  MPI_DEFS = -DHAVE_MPI=1
endif

if HAVE_PHOTON
  PHOTON_DEFS = -DHAVE_PHOTON=1
endif

if HAVE_NETWORK
  NETWORK_DEFS = -DHAVE_NETWORK=1
endif

AM_CPPFLAGS = $(MPI_DEFS) $(PHOTON_DEFS) $(NETWORK_DEFS)

endif
