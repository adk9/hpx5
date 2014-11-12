noinst_appsdir         = $(datarootdir)/apps/

noinst_apps_PROGRAMS   = fmmparcels

fmmparcels_SOURCES  = fmm-action.c  fmm-main.c  fmm-param.c

fmmparcels_LDFLAGS  = 
fmmparcels_LDADD    = $(top_builddir)/libhpx/libhpx.la -lm

fmmparcels_CPPFLAGS = -I$(top_srcdir)/include
fmmparcels_CFLAGS   = @MPI_CFLAGS@
fmmparcels_CFLAGS  += @HWLOC_CFLAGS@ @PAPI_CFLAGS@
