noinst_appsdir         = $(datarootdir)/apps/

noinst_apps_PROGRAMS   = luleshnewfutures

luleshnewfutures_SOURCES  = lulesh-comp.c  lulesh-hpx.c  lulesh-util.c  lulesh-xfer.c

luleshnewfutures_LDFLAGS  = 
luleshnewfutures_LDADD    = $(HPX_LDADD) -lm
luleshnewfutures_DEPENDENCIES = $(HPX_LDADD)

luleshnewfutures_CPPFLAGS = -I$(top_srcdir)/include
luleshnewfutures_CFLAGS   = @MPI_CFLAGS@
luleshnewfutures_CFLAGS  += @HWLOC_CFLAGS@ @PAPI_CFLAGS@

luleshnewfutures_CFLAGS  += -Wno-unused -Wno-implicit-function-declaration -Wno-unused-variable
luleshnewfutures_CFLAGS  += -Wno-uninitialized
