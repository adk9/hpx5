if HAVE_NETWORK
SUBDIRS = isir pwc
endif

# The networking library
noinst_LTLIBRARIES	   = libnetwork.la

libnetwork_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libnetwork_la_CFLAGS   = $(LIBHPX_CFLAGS)
libnetwork_la_SOURCES  = locality.c \
                         network.c \
                         parcel.c \
                         parcel_block.c \
                         hpx_parcel_glue.c \
                         probe.c \
                         smp.c \
                         commands.c

if HAVE_NETWORK
libnetwork_la_LIBADD   = isir/libisir.la \
                         pwc/libpwc.la
endif
