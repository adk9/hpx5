examplesdir				= $(datarootdir)/doc/@PACKAGE@

examples_PROGRAMS		= fibonacci
fibonacci_SOURCES		= fibonacci.c

fibonacci_CPPFLAGS		= -I$(top_srcdir)/include -I$(top_builddir)/include
fibonacci_LDADD			= $(top_builddir)/src/libhpx.la
