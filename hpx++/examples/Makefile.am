
noinst_PROGRAMS         = guppie

guppie_SOURCES	= guppie.cpp

#AM_CXXFLAGS      		= -D_GNU_SOURCE $(HPX_APPS_CPPFLAGS) -I$(top_srcdir)/include
AM_CFLAGS               = $(HPX_APPS_CFLAGS) -Wno-unused
AM_CXXFLAGS                     = $(HPX_APPS_CPPFLAGS) $(AM_CPPFLAGS) $(AM_CFLAGS) -std=c++11
AM_LDFLAGS       		= $(HPX_APPS_LDFLAGS) -no-install
LDADD            		= $(HPX_APPS_LDADD)

guppie_DEPENDENCIES	     = $(HPX_APPS_DEPS)
