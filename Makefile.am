####################
# Makefile for HPX #
####################

EXTRA_DIST = \
        autogen.sh           \
        README NEWS TODO     \
        examples/Makefile    \
        examples/fibonacci.c \
        docs

AUTOMAKE_OPTIONS= foreign
ACLOCAL_AMFLAGS = -I config

SUBDIRS         = libsync src include examples

if ENABLE_DOCS
    SUBDIRS    += docs
endif

if ENABLE_TESTS
    SUBDIRS    += tests
endif
