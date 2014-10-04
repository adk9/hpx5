noinst_appsdir         = $(datarootdir)/apps/

noinst_apps_PROGRAMS   = luleshnewfutures

luleshnewfutures_SOURCES  = lulesh-comp.c  lulesh-hpx.c  lulesh-util.c  lulesh-xfer.c

luleshnewfutures_LDFLAGS  = -static
luleshnewfutures_LDADD    = $(top_builddir)/libhpx/libhpx.la -lm

luleshnewfutures_CPPFLAGS = -I$(top_srcdir)/include
luleshnewfutures_CFLAGS   = @PHOTON_CFLAGS@ @MPI_CFLAGS@
luleshnewfutures_CFLAGS  += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

luleshnewfutures_CFLAGS  += -Wno-unused -Wno-implicit-function-declaration -Wno-unused-variable
luleshnewfutures_CFLAGS  += -Wno-uninitialized

if HAVE_PHOTON
luleshnewfutures_LDADD   += -lphoton
endif
