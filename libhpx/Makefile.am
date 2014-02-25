####################
# Makefile for HPX #
####################
SUBDIRS             = transport platform

lib_LTLIBRARIES		= libhpx.la

libhpx_la_SOURCES	= hpx.c locks.c locality.c scheduler.c parcel.c action.c \
					  thread.c network.c future.c

if CPU_X86_64
libhpx_la_SOURCES  += arch/x86_64/transfer.S arch/x86_64/asm.S
endif

libhpx_la_CPPFLAGS  = -I$(top_srcdir)/include

libhpx_la_CFLAGS	= $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99 \
					  @CRYPTO_CFLAGS@ @HWLOC_CFLAGS@ @PAPI_CFLAGS@ \
					  @TCMALLOC_CFLAGS@ @GLIB_CFLAGS@ @PMI_CFLAGS@ 

libhpx_la_LIBADD	= $(top_builddir)/libsync/libsync.la \
	      			   transport/libtransport.la platform/libplatform.la \
					  -lpthread @CRYPTO_LIBS@ @HWLOC_LIBS@ @PAPI_LIBS@ \
					  @TCMALLOC_LIBS@ @GLIB_LIBS@ @PMI_LIBS@

