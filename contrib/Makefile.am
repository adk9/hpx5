SUBDIRS       = jemalloc_build_global jemalloc_build_registered
DIST_SUBDIRS  = jemalloc_build_global jemalloc_build_registered

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
SUBDIRS      += jemalloc_build
DIST_SUBDIRS += jemalloc_build
endif

check-recursive: all-recursive
	echo "HPX ignore above warnings, we do not check contrib projects"
