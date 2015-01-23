####################
# Makefile for HPX #
####################

EXTRA_DIST = \
        autogen.sh           \
        README NEWS TODO     \
        INSTALL AUTHORS      \
        docs

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I config -I contrib/hwloc-1.10.0/config

SUBDIRS       = contrib libsync libhpx include examples bench
DIST_SUBDIRS  = contrib libsync libhpx include examples bench

if ENABLE_DOCS
SUBDIRS      += docs
DIST_SUBDIRS += docs
endif

if ENABLE_TESTS
SUBDIRS      += tests
DIST_SUBDIRS += tests
endif

if ENABLE_TUTORIAL
SUBDIRS      += tutorial
DIST_SUBDIRS += tutorial
endif
