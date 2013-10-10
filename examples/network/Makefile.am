examplesdir = $(datarootdir)/doc/@PACKAGE@

AM_CPPFLAGS = -I$(top_srcdir)/include

examples_PROGRAMS = pingpong
pingpong_SOURCES = pingpong.c
pingpong_LDADD = $(top_srcdir)/src/libhpx.la -lmpich -lmpl

