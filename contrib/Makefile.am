SUBDIRS       = jemalloc_build_hpx
DIST_SUBDIRS  = jemalloc_build_hpx

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
