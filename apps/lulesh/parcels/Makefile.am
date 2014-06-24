noinst_appsdir         = $(abs_top_builddir)/apps/

noinst_apps_PROGRAMS   = luleshparcels

luleshparcels_SOURCES  = lulesh-comp.c  lulesh-hpx.c  lulesh-util.c  lulesh-xfer.c

luleshparcels_LDADD    = $(top_builddir)/libhpx/libhpx.la -lm

luleshparcels_CPPFLAGS = -I$(top_srcdir)/include
luleshparcels_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
luleshparcels_CFLAGS   += @PHOTON_CFLAGS@ @MPI_CFLAGS@
luleshparcels_CFLAGS   += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

luleshparcels_LDFLAGS  = -static
