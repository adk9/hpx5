examplesdir = $(datarootdir)/doc/@PACKAGE@

AM_CPPFLAGS = -I$(top_srcdir)/include

examples_PROGRAMS = fibonacci
fibonacci_SOURCES = fibonacci.c
fibonacci_LDADD = $(top_srcdir)/src/libhpx.la -lmpich -lmpl

