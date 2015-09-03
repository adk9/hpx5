parceldir                     = $(datarootdir)/doc/@PACKAGE@
parcel_PROGRAMS               = parcelSend parcelSendSync

AM_CPPFLAGS      		= -D_GNU_SOURCE $(HPX_APPS_CPPFLAGS) -I$(top_srcdir)/include
AM_CFLAGS               = $(HPX_APPS_CFLAGS) -Wno-unused
AM_LDFLAGS       		= -no-install
LDADD            		= $(HPX_APPS_LDADD)

parcelSend_SOURCES            = parcelSend.c 
parcelSendSync_SOURCES        = parcelSendSync.c

parcelSend_DEPENDENCIES       = $(HPX_APPS_DEPS)
parcelSendSync_DEPENDENCIES   = $(HPX_APPS_DEPS)
