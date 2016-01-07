include_HEADERS = nbc.h

sources = \
	$(include_HEADERS) \
	nbc.c \
	nbc_iallgather.c \
	nbc_iallgatherv.c \
	nbc_iallreduce.c \
	nbc_ialltoall.c \
	nbc_ialltoallv.c \
	nbc_ibarrier.c \
	nbc_ibcast.c \
	nbc_ibcast_inter.c \
	nbc_igather.c \
	nbc_igatherv.c \
	nbc_ireduce.c \
	nbc_ireduce_scatter.c \
	nbc_iscan.c \
	nbc_iscatter.c \
	nbc_iscatterv.c \
	nbc_op.c \
	nbc_internal.h \
	LICENSE \
	CHANGELOG

noinst_HEADERS = nbc_internal.h 

if HAVE_PHOTON
phdir = ph
phlibs = ph/libph.la
endif

tstdir = test
#tstdir = 

SUBDIRS = libdict $(phdir) . $(tstdir)

lib_LTLIBRARIES = libnbc.la

libnbc_la_SOURCES = $(sources)
libnbc_la_LIBADD = libdict/libdict.la $(phlibs) 
libnbc_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libnbc_la_CFLAGS   = $(LIBHPX_CFLAGS)

