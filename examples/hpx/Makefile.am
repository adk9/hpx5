examplesdir = $(datarootdir)/doc/@PACKAGE@

AM_CPPFLAGS = -I$(top_srcdir)/include

examples_PROGRAMS = fibonaccihpx pingponghpx allreduce
fibonaccihpx_SOURCES = fibonacci.c
fibonaccihpx_LDADD = $(top_srcdir)/src/libhpx.la
pingponghpx_SOURCES = pingpong.c
pingponghpx_LDADD = $(top_srcdir)/src/libhpx.la
allreduce_SOURCES = allreduce.c
allreduce_LDADD = $(top_srcdir)/src/libhpx.la