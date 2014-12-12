/*
 * Name mangling for public symbols is controlled by --with-mangling and
 * --with-jemalloc-prefix.  With default settings the je_ prefix is stripped by
 * these macro definitions.
 */
#define TOKENPASTE_HELPER(x, y) x ## y
#define TOKENPASTE(x, y) TOKENPASTE_HELPER(x, y)
#ifndef JEMALLOC_NO_RENAME
#  define je_no 
#  define je_malloc_conf TOKENPASTE(JEMALLOC_RENAME, malloc_conf)
#  define je_malloc_message TOKENPASTE(JEMALLOC_RENAME, malloc_message)
#  define je_malloc TOKENPASTE(JEMALLOC_RENAME, malloc)
#  define je_calloc TOKENPASTE(JEMALLOC_RENAME, calloc)
#  define je_posix_memalign TOKENPASTE(JEMALLOC_RENAME, posix_memalign)
#  define je_aligned_alloc TOKENPASTE(JEMALLOC_RENAME, aligned_alloc)
#  define je_realloc TOKENPASTE(JEMALLOC_RENAME, realloc)
#  define je_free TOKENPASTE(JEMALLOC_RENAME, free)
#  define je_mallocx TOKENPASTE(JEMALLOC_RENAME, mallocx)
#  define je_rallocx TOKENPASTE(JEMALLOC_RENAME, rallocx)
#  define je_xallocx TOKENPASTE(JEMALLOC_RENAME, xallocx)
#  define je_sallocx TOKENPASTE(JEMALLOC_RENAME, sallocx)
#  define je_dallocx TOKENPASTE(JEMALLOC_RENAME, dallocx)
#  define je_sdallocx TOKENPASTE(JEMALLOC_RENAME, sdallocx)
#  define je_nallocx TOKENPASTE(JEMALLOC_RENAME, nallocx)
#  define je_mallctl TOKENPASTE(JEMALLOC_RENAME, mallctl)
#  define je_mallctlnametomib TOKENPASTE(JEMALLOC_RENAME, mallctlnametomib)
#  define je_mallctlbymib TOKENPASTE(JEMALLOC_RENAME, mallctlbymib)
#  define je_malloc_stats_print TOKENPASTE(JEMALLOC_RENAME, malloc_stats_print)
#  define je_malloc_usable_size TOKENPASTE(JEMALLOC_RENAME, malloc_usable_size)
#  define je_memalign TOKENPASTE(JEMALLOC_RENAME, memalign)
#  define je_valloc TOKENPASTE(JEMALLOC_RENAME, valloc)
#endif
