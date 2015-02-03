SUBDIRS =

if HAVE_MPI
SUBDIRS += isir
endif

if HAVE_PHOTON
SUBDIRS += pwc
endif

# The networking library
noinst_LTLIBRARIES	   = libnetwork.la

libnetwork_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/contrib
libnetwork_la_CFLAGS   = 
libnetwork_la_SOURCES  = locality.c network.c parcel.c parcel_queue.c \
                         progress.c probe.c smp.c \
                         routing/routing.c routing/dummy.c 
libnetwork_la_LIBADD   =

if HAVE_MPI
libnetwork_la_LIBADD  += isir/libisir.la
endif

if HAVE_PHOTON
libnetwork_la_LIBADD  += pwc/libpwc.la
endif

