parceldir                     = $(datarootdir)/doc/@PACKAGE@
parcel_PROGRAMS               = parcelSend parcelSendSync

parcelSend_SOURCES            = parcelSend.c 
parcelSendSync_SOURCES        = parcelSendSync.c

all_LIBS                    = $(HPX_JEMALLOC_LIBS) $(HPX_LIBS)

AM_CPPFLAGS                 = -D_GNU_SOURCE -I$(top_srcdir)/include
AM_CFLAGS                   =
AM_LDFLAGS                  = $(HPX_JEMALLOC_LDFLAGS) -no-install

if HAVE_MPI
AM_CFLAGS                    += @MPI_CFLAGS@
endif

parcelSend_LDADD              = $(all_LIBS)
parcelSendSync_LDADD          = $(all_LIBS)

parcelSend_DEPENDENCIES       = $(HPX_DEPS)
parcelSendSync_DEPENDENCIES   = $(HPX_DEPS)
