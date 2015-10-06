####################
# Makefile for HPX #
####################

EXTRA_DIST = \
        README NEWS          \
        INSTALL AUTHORS      \
        docs

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I config -I contrib/hwloc-1.11.0/config

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = hpx.pc

SUBDIRS       = contrib libsync libhpx include examples
DIST_SUBDIRS  = contrib libsync libhpx include examples

if HAVE_APEX
SUBDIRS      += libapex
DIST_SUBDIRS += libapex
endif

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

if HAVE_HPXPP
SUBDIRS      += hpx++
DIST_SUBDIRS += hpx++
endif

bin_SCRIPTS = scripts/hpx-config

install-exec-hook:
	cd $(DESTDIR)$(libdir); find $(DESTDIR)$(libdir) -type f -name \*.la -delete
