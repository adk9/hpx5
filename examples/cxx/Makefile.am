noinst_PROGRAMS = guppie fibonacci
guppie_SOURCES	= guppie.cpp
fibonacci_SOURCES = fibonacci.cpp

AM_CPPFLAGS     = $(HPX_APPS_CPPFLAGS) -I$(top_srcdir)/include
AM_CXXFLAGS     = $(HPX_APPS_CXXFLAGS)
AM_LDFLAGS      = $(HPX_APPS_LDFLAGS) -no-install
LDADD           = $(HPX_APPS_LDADD)

guppie_DEPENDENCIES	     = $(HPX_APPS_DEPS)
fibonacci_DEPENDENCIES	     = $(HPX_APPS_DEPS)
