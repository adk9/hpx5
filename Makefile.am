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

if HAVE_MPI
 SUBDIRS += test
endif

if ENABLE_TESTS
SUBDIRS += tests
endif

#if HAVE_VERBS
#SUBDIRS += src/verbs
#endif

#if HAVE_UGNI
#SUBDIRS += src/ugni
#endif

#SUBDIRS += src/libphoton test
