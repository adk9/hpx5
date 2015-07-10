# LT Utility libraries
noinst_LTLIBRARIES		= libdarwin.la

libdarwin_la_CPPFLAGS	= -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libdarwin_la_CFLAGS		= $(LIBHPX_CFLAGS)
libdarwin_la_SOURCES	= time.c cpu.c mmap.c usleep.c barrier.c
