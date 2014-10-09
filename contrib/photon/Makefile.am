#######################
# Makefile for Photon #
#######################

instdir = $(prefix)/include
inst_HEADERS=include/photon.h include/photon_io.h include/photon_xsp_forwarder.h

EXTRA_DIST = autogen.sh      \
        README NEWS TODO     \
        docs

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS= -I config
SUBDIRS = . src/util src
DIST_SUBDIRS = . src/util src

if PHOTON_ENABLE_TESTS
if HAVE_MPI
 SUBDIRS += test
 DIST_SUBDIRS += test
endif
SUBDIRS += tests
DIST_SUBDIRS += tests
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = photon.pc

#if HAVE_VERBS
#SUBDIRS += src/verbs
#endif

#if HAVE_UGNI
#SUBDIRS += src/ugni
#endif

#SUBDIRS += src/libphoton test
