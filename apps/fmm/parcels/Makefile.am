noinst_appsdir         = $(abs_top_builddir)/apps/

noinst_apps_PROGRAMS   = fmmparcels

fmmparcels_SOURCES  = fmm-action.c  fmm-dag.c  fmm-main.c  fmm-param.c

fmmparcels_LDFLAGS  = -static
fmmparcels_LDADD    = $(top_builddir)/libhpx/libhpx.la -lm

fmmparcels_CPPFLAGS = -I$(top_srcdir)/include
fmmparcels_CFLAGS   = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
fmmparcels_CFLAGS  += @PHOTON_CFLAGS@ @MPI_CFLAGS@
fmmparcels_CFLAGS  += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

if HAVE_PHOTON
fmmparcels_LDADD   += -lphoton -ljemalloc
endif
