#######################
# Makefile for Photon #
#######################

instdir = $(prefix)/include
inst_HEADERS=include/photon.h

EXTRA_DIST = autogen.sh      \
        README NEWS TODO     \
        docs

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS= -I config
SUBDIRS = . src/util src test

#if HAVE_VERBS
#SUBDIRS += src/verbs
#endif

#if HAVE_UGNI
#SUBDIRS += src/ugni
#endif

#SUBDIRS += src/libphoton test
