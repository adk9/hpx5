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

if BUILD_JEMALLOC
SUBDIRS      += jemalloc
DIST_SUBDIRS += jemalloc
endif

if BUILD_LIBCUCKOO
SUBDIRS      += libcuckoo
DIST_SUBDIRS += libcuckoo
endif

check-recursive: all-recursive
	echo "HPX ignore above warnings, we do not check contrib projects"
