####################
# Makefile for HPX #
####################

EXTRA_DIST = \
        autogen.sh           \
        README NEWS TODO     \
        docs

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I config -I contrib/hwloc-1.9.1/config

SUBDIRS = contrib libsync libhpx include examples bench

if ENABLE_DOCS
SUBDIRS += docs
endif

if ENABLE_TESTS
SUBDIRS += tests
endif

if ENABLE_APPS
SUBDIRS += apps
endif
