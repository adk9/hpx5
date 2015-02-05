####################
# Makefile for HPX #
####################

EXTRA_DIST = \
        README NEWS          \
        INSTALL AUTHORS      \
        docs

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I config -I contrib/hwloc-1.10.0/config

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = hpx.pc

SUBDIRS       = contrib libsync libhpx include examples bench
DIST_SUBDIRS  = contrib libsync libhpx include examples bench tests docs tutorial

if ENABLE_DOCS
SUBDIRS      += docs
endif

if ENABLE_TESTS
SUBDIRS      += tests
endif

if ENABLE_TUTORIAL
SUBDIRS      += tutorial
endif
