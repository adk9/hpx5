# LT Utility libraries
noinst_LTLIBRARIES	   = libsystem.la

# libsystem files and flags
libsystem_la_CPPFLAGS  = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libsystem_la_CFLAGS    = $(LIBHPX_CFLAGS)
libsystem_la_SOURCES   = 
libsystem_la_LIBADD    =

if OS_LINUX
libsystem_la_CPPFLAGS += -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
libsystem_la_SOURCES  += linux/time.c linux/cpu.c linux/mmap.c
libsystem_la_LIBADD   += -lrt
endif

if OS_DARWIN
libsystem_la_SOURCES  += darwin/time.c darwin/cpu.c darwin/barrier.c darwin/mmap.c
endif

