# LT Utility libraries
noinst_LTLIBRARIES   = libutil.la

# libutil files and flags
libutil_la_CPPFLAGS  = -I$(top_srcdir)/include

libutil_la_SOURCES   = parser.c options.c hashstr.c bitmap.c
libutil_la_LIBADD    =

if HAVE_APEX
libutil_la_CPPFLAGS  += @APEX_CFLAGS@
libutil_la_LIBADD  += @APEX_LIBS@
endif
