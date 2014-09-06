noinst_appsdir         = $(datarootdir)/apps/

noinst_apps_PROGRAMS   = luleshparcels

luleshparcels_SOURCES  = lulesh-comp.c  lulesh-hpx.c  lulesh-util.c  lulesh-xfer.c

luleshparcels_LDFLAGS  = -static
luleshparcels_LDADD    = $(top_builddir)/libhpx/libhpx.la -lm

luleshparcels_CPPFLAGS = -I$(top_srcdir)/include
luleshparcels_CFLAGS   = @PHOTON_CFLAGS@ @MPI_CFLAGS@
luleshparcels_CFLAGS  += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

if HAVE_PHOTON
luleshparcels_LDADD   += -lphoton -ljemalloc
endif
