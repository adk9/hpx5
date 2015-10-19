noinst_LTLIBRARIES  = libarch.la

libarch_la_CPPFLAGS  = -I$(top_srcdir)/include -I$(top_srcdir)/src/contrib/libsync
libarch_la_CFLAGS    = $(LIBPHOTON_CFLAGS)
libarch_la_CCASFLAGS = $(LIBPHOTON_CCASFLAGS)

if CPU_X86_64
libarch_la_SOURCES  = x86_64/nop.S
if HAVE_CMPXCHG16B
libarch_la_SOURCES += x86_64/cptr.S
endif
endif

if CPU_ARM
libarch_la_SOURCES = common/nop.c arm/cptr.S
endif

if CPU_AARCH64
libarch_la_SOURCES = common/nop.c
endif
