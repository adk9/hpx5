noinst_appsdir         = $(datarootdir)/apps/

noinst_apps_PROGRAMS   = luleshparcels

luleshparcels_SOURCES  = lulesh-comp.c  lulesh-hpx.c  lulesh-util.c  lulesh-xfer.c

luleshparcels_LDFLAGS  = -static
luleshparcels_LDADD    = $(top_builddir)/libhpx/libhpx.la -lm

luleshparcels_CPPFLAGS = -I$(top_srcdir)/include
luleshparcels_CFLAGS   = @PHOTON_CFLAGS@ @MPI_CFLAGS@
luleshparcels_CFLAGS  += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

luleshparcels_CFLAGS  += -Wno-unused -Wno-implicit-function-declaration -Wno-unused-variable
luleshparcels_CFLAGS  += -Wno-uninitialized

if HAVE_PHOTON
luleshparcels_LDADD   += -lphoton
endif
