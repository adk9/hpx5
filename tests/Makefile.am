if WITH_TESTSUITE

TESTS = hpxtest
check_PROGRAMS = hpxtest
hpxtest_SOURCES = 00_hpxtest.c
hpxtest_CFLAGS = @CHECK_CFLAGS@ @PAPI_CFLAGS@
hpxtest_LDADD = $(top_builddir)/src/libhpx.la -lm -lrt -lpthread -lcheck @CHECK_LIBS@ @PAPI_LIBS@

if CPU_X86_64
  ARCH_SRC = arch/x86_64/99_hpxtest.S
endif

hpxtest_SOURCES += $(ARCH_SRC)

endif