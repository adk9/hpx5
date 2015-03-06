SUBDIRS = isir pwc

# The networking library
noinst_LTLIBRARIES	   = libnetwork.la

libnetwork_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/contrib
libnetwork_la_CFLAGS   = 
libnetwork_la_SOURCES  = locality.c network.c parcel.c parcel_queue.c \
                         progress.c probe.c smp.c \
                         routing/routing.c routing/dummy.c 
libnetwork_la_LIBADD   = isir/libisir.la pwc/libpwc.la
