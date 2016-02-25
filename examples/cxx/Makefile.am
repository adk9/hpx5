noinst_PROGRAMS = guppie fibonacci hello
guppie_SOURCES	= guppie.cpp
fibonacci_SOURCES = fibonacci.cpp
hello_SOURCES 	= hello.cpp

AM_CPPFLAGS     = $(HPX_APPS_CPPFLAGS) -I$(top_srcdir)/include
AM_CXXFLAGS     = $(HPX_APPS_CXXFLAGS)
AM_LDFLAGS      = $(HPX_APPS_LDFLAGS) -no-install
LDADD           = $(HPX_APPS_LDADD)

guppie_DEPENDENCIES	     = $(HPX_APPS_DEPS)
fibonacci_DEPENDENCIES	     = $(HPX_APPS_DEPS)
hello_DEPENDENCIES	     = $(HPX_APPS_DEPS)
