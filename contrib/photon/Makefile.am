#######################
# Makefile for Photon #
#######################

instdir = $(prefix)/include
inst_HEADERS=include/photon.h                 \
	     include/photon_config.h          \
	     include/photon_api.h             \
	     include/photon_io.h              \
	     include/photon_xsp_forwarder.h   \
	     include/photon_attributes.h

EXTRA_DIST = autogen.sh      \
        README NEWS TODO     \
        docs

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS= -I config
SUBDIRS = src
DIST_SUBDIRS = src

if ENABLE_TESTS
if HAVE_MPI
 SUBDIRS += test
 DIST_SUBDIRS += test
endif
SUBDIRS += tests
DIST_SUBDIRS += tests
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = photon.pc
