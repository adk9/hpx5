# LT Utility libraries
noinst_LTLIBRARIES = libaffinity.la

# libgas files and flags
libaffinity_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libaffinity_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libaffinity_la_SOURCES  = affinity.cc none.cc cuckoo_hash.cc
libaffinity_la_LIBADD   = 

if HAVE_URCU
libaffinity_la_SOURCES += urcu_map.cc
endif
