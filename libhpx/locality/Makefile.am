SUBDIRS = platform 

# LT Utility libraries
noinst_LTLIBRARIES	  = liblocality.la

# liblocality files and flags
liblocality_la_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/include/libhpx -I$(srcdir)/..
liblocality_la_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99
liblocality_la_SOURCES  = locality.c smp.c manager.c
liblocality_la_LIBADD   = platform/libplatform.la

if HAVE_MPI
liblocality_la_CFLAGS  += @MPI_CFLAGS@
liblocality_la_SOURCES += mpirun.c
liblocality_la_LIBADD  += @MPI_LIBS@
endif

if HAVE_PMI
liblocality_la_CFLAGS  += @PMI_CFLAGS@
liblocality_la_SOURCES += pmi.c
liblocality_la_LIBADD  += @PMI_LIBS@
endif

if HAVE_HWLOC
liblocality_la_CFLAGS  += @HWLOC_CFLAGS@
liblocality_la_LIBADD  += @HWLOC_LIBS@
endif

if HAVE_CRYPTO
liblocality_la_CFLAGS  += @CRYPTO_CFLAGS@
liblocality_la_LIBADD  += @CRYPTO_LIBS@
endif

if HAVE_GLIB
liblocality_la_CFLAGS  += @GLIB_CFLAGS@
liblocality_la_LIBADD  += @GLIB_LIBS@
endif
