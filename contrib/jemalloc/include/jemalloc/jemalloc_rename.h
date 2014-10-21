/*
 * Name mangling for public symbols is controlled by --with-mangling and
 * --with-jemalloc-prefix.  With default settings the je_ prefix is stripped by
 * these macro definitions.
 */
#ifndef JEMALLOC_NO_RENAME
#  define je_no 
#  define je_malloc_conf libhpx_global_malloc_conf
#  define je_malloc_message libhpx_global_malloc_message
#  define je_malloc libhpx_global_malloc
#  define je_calloc libhpx_global_calloc
#  define je_posix_memalign libhpx_global_posix_memalign
#  define je_aligned_alloc libhpx_global_aligned_alloc
#  define je_realloc libhpx_global_realloc
#  define je_free libhpx_global_free
#  define je_mallocx libhpx_global_mallocx
#  define je_rallocx libhpx_global_rallocx
#  define je_xallocx libhpx_global_xallocx
#  define je_sallocx libhpx_global_sallocx
#  define je_dallocx libhpx_global_dallocx
#  define je_sdallocx libhpx_global_sdallocx
#  define je_nallocx libhpx_global_nallocx
#  define je_mallctl libhpx_global_mallctl
#  define je_mallctlnametomib libhpx_global_mallctlnametomib
#  define je_mallctlbymib libhpx_global_mallctlbymib
#  define je_malloc_stats_print libhpx_global_malloc_stats_print
#  define je_malloc_usable_size libhpx_global_malloc_usable_size
#  define je_memalign libhpx_global_memalign
#  define je_valloc libhpx_global_valloc
#endif
