# LT Utility libraries
noinst_LTLIBRARIES	   = libsystem.la

# libsystem files and flags
libsystem_la_CPPFLAGS  = -I$(top_srcdir)/include
libsystem_la_CFLAGS    = $(HPX_PEDANTIC) $(HPX_W_ALL) $(HPX_W_ERROR) $(HPX_C99)
libsystem_la_SOURCES   = null.c
libsystem_la_LIBADD    =

if OS_LINUX
libsystem_la_CPPFLAGS += -D_POSIX_C_SOURCE=200809L
libsystem_la_SOURCES  += linux/time.c linux/cpu.c
libsystem_la_LIBADD   += -lrt
endif

if OS_DARWIN
libsystem_la_SOURCES  += darwin/time.c darwin/cpu.c
endif

if HAVE_HWLOC
libsystem_la_CFLAGS   += @HWLOC_CFLAGS@
libsystem_la_LIBADD   += @HWLOC_LIBS@
endif

if HAVE_CRYPTO
libsystem_la_CFLAGS   += @CRYPTO_CFLAGS@
libsystem_la_LIBADD   += @CRYPTO_LIBS@
endif

if HAVE_GLIB
libsystem_la_CFLAGS   += @GLIB_CFLAGS@
libsystem_la_LIBADD   += @GLIB_LIBS@
endif
