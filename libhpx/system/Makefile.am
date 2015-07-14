# LT Utility libraries
noinst_LTLIBRARIES	= libsystem.la

libsystem_la_SOURCES =

if OS_LINUX
SUBDIRS = linux
libsystem_la_LIBADD	= linux/liblinux.la
endif

if OS_DARWIN
SUBDIRS = darwin
libsystem_la_LIBADD	= darwin/libdarwin.la
endif

