parceldir                     = $(datarootdir)/doc/@PACKAGE@
parcel_PROGRAMS               = parcelSend parcelSendSync

AM_CPPFLAGS			= -D_GNU_SOURCE $(LIBFFI_CFLAGS) -I$(top_srcdir)/include
AM_LDFLAGS			= $(LIBHPX_LDFLAGS) -no-install
LDADD				= $(LIBHPX_LDADD)

parcelSend_SOURCES            = parcelSend.c 
parcelSendSync_SOURCES        = parcelSendSync.c

parcelSend_DEPENDENCIES       = $(LIBHPX_DEPS)
parcelSendSync_DEPENDENCIES   = $(LIBHPX_DEPS)
