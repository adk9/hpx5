# The AGAS library
noinst_LTLIBRARIES  = libagas.la

noinst_HEADERS      = AGAS.h BlockTranslationTable.h ChunkTable.h GlobalVirtualAddress.h Rebalancer.h BlockStatisticsTable.h
libagas_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBHPX_CPPFLAGS)
libagas_la_CXXFLAGS = $(LIBHPX_CXXFLAGS)
libagas_la_SOURCES  = BlockTranslationTable.cpp ChunkTable.cpp \
                      ChunkAllocator.cpp AGAS.cpp Rebalancer.cpp \
                      BlockStatisticsTable.cpp partitioning.cpp

if HAVE_JEMALLOC
libagas_la_SOURCES += jemalloc.cpp
endif

if HAVE_TBBMALLOC
libagas_la_SOURCES += tbbmalloc.cpp
endif

if HAVE_DLMALLOC
libagas_la_SOURCES += dlmalloc.cpp
endif
