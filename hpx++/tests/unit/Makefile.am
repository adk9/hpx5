
noinst_PROGRAMS         = globalp future_test action_tests

globalp_SOURCES		     = globalp.cpp
future_test_SOURCES 	= future_test.cpp
action_tests_SOURCES	= action_tests.cpp

AM_CPPFLAGS     = $(HPX_APPS_CPPFLAGS) -I$(top_srcdir)/include
AM_CXXFLAGS     = $(HPX_APPS_CXXFLAGS)
AM_LDFLAGS      = $(HPX_APPS_LDFLAGS) -no-install
LDADD           = $(HPX_APPS_LDADD)

globalp_DEPENDENCIES		= $(HPX_APPS_DEPS)
future_test_DEPENDENCIES	= $(HPX_APPS_DEPS)
action_tests_DEPENDENCIES	= $(HPX_APPS_DEPS)

