/*
 * Name mangling for public symbols is controlled by --with-mangling and
 * --with-jemalloc-prefix.  With default settings the je_ prefix is stripped by
 * these macro definitions.
 */
#ifndef JEMALLOC_NO_RENAME
#  define je_no 
#  define je_malloc_conf libhpx_malloc_conf
#  define je_malloc_message libhpx_malloc_message
#  define je_malloc libhpx_malloc
#  define je_calloc libhpx_calloc
#  define je_posix_memalign libhpx_posix_memalign
#  define je_aligned_alloc libhpx_aligned_alloc
#  define je_realloc libhpx_realloc
#  define je_free libhpx_free
#  define je_mallocx libhpx_mallocx
#  define je_rallocx libhpx_rallocx
#  define je_xallocx libhpx_xallocx
#  define je_sallocx libhpx_sallocx
#  define je_dallocx libhpx_dallocx
#  define je_sdallocx libhpx_sdallocx
#  define je_nallocx libhpx_nallocx
#  define je_mallctl libhpx_mallctl
#  define je_mallctlnametomib libhpx_mallctlnametomib
#  define je_mallctlbymib libhpx_mallctlbymib
#  define je_malloc_stats_print libhpx_malloc_stats_print
#  define je_malloc_usable_size libhpx_malloc_usable_size
#  define je_memalign libhpx_memalign
#  define je_valloc libhpx_valloc
#endif
