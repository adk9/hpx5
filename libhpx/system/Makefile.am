# LT Utility libraries
noinst_LTLIBRARIES	= libsystem.la

libsystem_la_SOURCES =

if OS_LINUX
SUBDIRS = linux
libsystem_la_LIBADD	= linux/liblinux.la
endif

if OS_DARWIN
SUBDIRS = linux
libsystem_la_LIBADD	= linux/libdarwin.la
endif

