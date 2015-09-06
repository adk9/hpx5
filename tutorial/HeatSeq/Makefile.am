HeatSeqdir                      = $(datarootdir)/doc/@PACKAGE@
HeatSeq_PROGRAMS                = heat_1

AM_CPPFLAGS      		= -D_GNU_SOURCE $(HPX_APPS_CPPFLAGS) -I$(top_srcdir)/include
AM_CFLAGS               = $(HPX_APPS_CFLAGS) -Wno-unused
AM_LDFLAGS       		= $(HPX_APPS_LDFLAGS) -no-install
LDADD            		= $(HPX_APPS_LDADD)

heat_1_SOURCES                  = heat_1.c 
heat_1_DEPENDENCIES             = $(HPX_APPS_DEPS)
