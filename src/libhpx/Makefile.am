####################
# Makefile for HPX #
####################

lib_LTLIBRARIES = libhpx.la

libhpx_la_SOURCES = hpx_error.c hpx_init.c hpx_ctx.c hpx_thread.c hpx_queue.c hpx_kthread.c hpx_config.c hpx_list.c hpx_map.c

if CPU_X86_64
  ARCH_SRC = arch/x86_64/hpx_mctx.S arch/x86_64/hpx_mconfig.S arch/x86_64/hpx_mctx_mk.c arch/x86_64/hpx_lco_future.S

  libhpx_la_arch_x86_64dir = $(includedir)/hpx/arch/x86_64
  libhpx_la_arch_x86_64_HEADERS = $(top_srcdir)/include/arch/x86_64/*.h
endif

libhpx_la_SOURCES += $(ARCH_SRC)

libhpx_ladir = $(includedir)/hpx
nodist_libhpx_la_HEADERS = $(top_srcdir)/include/config.h
libhpx_la_HEADERS = $(top_srcdir)/include/*.h
