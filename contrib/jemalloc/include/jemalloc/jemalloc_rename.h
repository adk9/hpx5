/*
 * Name mangling for public symbols is controlled by --with-mangling and
 * --with-jemalloc-prefix.  With default settings the je_ prefix is stripped by
 * these macro definitions.
 */
#ifndef JEMALLOC_NO_RENAME
#  define je_malloc_conf hpx_malloc_conf
#  define je_malloc_message hpx_malloc_message
#  define je_malloc hpx_malloc
#  define je_calloc hpx_calloc
#  define je_posix_memalign hpx_posix_memalign
#  define je_aligned_alloc hpx_aligned_alloc
#  define je_realloc hpx_realloc
#  define je_free hpx_free
#  define je_mallocx hpx_mallocx
#  define je_rallocx hpx_rallocx
#  define je_xallocx hpx_xallocx
#  define je_sallocx hpx_sallocx
#  define je_dallocx hpx_dallocx
#  define je_sdallocx hpx_sdallocx
#  define je_nallocx hpx_nallocx
#  define je_mallctl hpx_mallctl
#  define je_mallctlnametomib hpx_mallctlnametomib
#  define je_mallctlbymib hpx_mallctlbymib
#  define je_malloc_stats_print hpx_malloc_stats_print
#  define je_malloc_usable_size hpx_malloc_usable_size
#  define je_memalign hpx_memalign
#  define je_valloc hpx_valloc
#endif
