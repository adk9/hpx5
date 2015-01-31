parceldir                     = $(datarootdir)/doc/@PACKAGE@
parcel_PROGRAMS               = parcelSend parcelSendSync

parcelSend_SOURCES            = parcelSend.c 
parcelSendSync_SOURCES        = parcelSendSync.c

all_LIBS                      = $(HPX_LDADD)

AM_CPPFLAGS                   = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                     =

if HAVE_MPI
AM_CFLAGS                    += @MPI_CFLAGS@
endif

parcelSend_LDADD              = $(all_LIBS)
parcelSendSync_LDADD          = $(all_LIBS)

parcelSend_DEPENDENCIES       = $(all_LIBS)
parcelSendSync_DEPENDENCIES   = $(all_LIBS)
