# LT Utility libraries
noinst_LTLIBRARIES	= libplatform.la

# libplatform files and flags
libplatform_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(top_srcdir)/libhpx
libplatform_la_CFLAGS	 = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) -std=c99

if OS_LINUX
libplatform_la_CPPFLAGS += -D_POSIX_C_SOURCE=200809L
libplatform_la_SOURCES	 = linux/time.c
libplatform_la_LIBADD	 = -lrt
endif
