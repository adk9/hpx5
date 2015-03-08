SUBDIRS       =
DIST_SUBDIRS  =

if BUILD_LIBFFI
SUBDIRS      += libffi
DIST_SUBDIRS += libffi
endif

if BUILD_HWLOC
SUBDIRS      += hwloc-1.10.0
DIST_SUBDIRS += hwloc-1.10.0
endif

if BUILD_PHOTON
SUBDIRS      += photon
DIST_SUBDIRS += photon
endif

if ENABLE_JEMALLOC
SUBDIRS      += jemalloc_build_local
DIST_SUBDIRS += jemalloc_build_local
endif

if BUILD_JEMALLOC_GLOBAL
SUBDIRS      += jemalloc_build_global
DIST_SUBDIRS += jemalloc_build_global
endif

if BUILD_JEMALLOC_REGISTERED
SUBDIRS      += jemalloc_build_registered
DIST_SUBDIRS += jemalloc_build_registered
endif

check-recursive: all-recursive
	echo "HPX ignore above warnings, we do not check contrib projects"
