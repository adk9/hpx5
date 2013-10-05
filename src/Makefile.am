####################
# Makefile for HPX #
####################

lib_LTLIBRARIES          = libhpx.la

libhpx_la_SOURCES        = runtime/ctx.c     \
                           runtime/init.c    \
                           thread/thread.c   \
                           thread/kthread.c  \
                           util/error.c      \
                           util/timer.c      \
                           util/config.c     \
                           util/map.c        \
                           sync/locks.c

if CPU_X86_64
libhpx_la_arch_x86_64_HEADERS = $(top_srcdir)/include/hpx/arch/x86_64/*.h
libhpx_la_arch_x86_64dir = $(includedir)/hpx/arch/x86_64
ARCH_SRC                 = thread/arch/x86_64/mctx.S       \
                           thread/arch/x86_64/mconfig.S    \
                           thread/arch/x86_64/mctx_mk.c \
                           arch/x86_64/sync/nop.c
endif

libhpx_la_SOURCES       += $(ARCH_SRC)

libhpx_ladir             = $(includedir)/hpx
nodist_libhpx_la_HEADERS = $(top_builddir)/include/config.h
libhpx_la_HEADERS        = $(top_srcdir)/include/hpx/*.h
libhpx_la_CPPFLAGS       = -I$(top_srcdir)/include -std=c99
libhpx_la_CFLAGS         = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_F_LTO) @PAPI_CFLAGS@ @HWLOC_CFLAGS@ @TCMALLOC_CFLAGS@
libhpx_la_LIBADD         = @PAPI_LIBS@ @HWLOC_LIBS@ @TCMALLOC_LIBS@

if PLATFORM_LINUX
libhpx_la_LIBADD += -lrt
endif

if HAVE_TCMALLOC
libhpx_la_CFLAGS += @TCMALLOC_CFLAGS@
libhpx_la_LIBADD += @TCMALLOC_LIBS@
endif
