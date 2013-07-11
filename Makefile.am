####################
# Makefile for HPX #
####################

EXTRA_DIST = autogen.sh      \
        README NEWS TODO     \
        examples/Makefile    \
        examples/fibonacci.c \
        docs

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS= -I config
SUBDIRS = . src examples

if WITH_DOCS
SUBDIRS += docs
endif

if WITH_TESTSUITE
SUBDIRS += tests
endif
