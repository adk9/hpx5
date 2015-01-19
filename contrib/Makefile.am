SUBDIRS      = libffi jemalloc_hpx
DIST_SUBDIRS = libffi jemalloc_hpx

if BUILD_HWLOC
SUBDIRS      += hwloc-1.10.0
DIST_SUBDIRS += hwloc-1.10.0
endif

if ENABLE_PHOTON
SUBDIRS      += photon
DIST_SUBDIRS += photon
endif

if ENABLE_JEMALLOC
SUBDIRS      += jemalloc
DIST_SUBDIRS += jemalloc
endif

check-recursive: all-recursive
	echo "HPX ignore above warnings, we do not check contrib projects"
