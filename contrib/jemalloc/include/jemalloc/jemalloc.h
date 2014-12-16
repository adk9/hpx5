#ifndef JEMALLOC_H_
#define	JEMALLOC_H_
#ifdef __cplusplus
extern "C" {
#endif

/* Defined if __attribute__((...)) syntax is supported. */
#define	JEMALLOC_HAVE_ATTR

/*
 * Define overrides for non-standard allocator-related functions if they are
 * present on the system.
 */
#define	JEMALLOC_OVERRIDE_MEMALIGN
#define	JEMALLOC_OVERRIDE_VALLOC

/*
 * At least Linux omits the "const" in:
 *
 *   size_t malloc_usable_size(const void *ptr);
 *
 * Match the operating system's prototype.
 */
#define	JEMALLOC_USABLE_SIZE_CONST

/* sizeof(void *) == 2^LG_SIZEOF_PTR. */
#define	LG_SIZEOF_PTR 3

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

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <strings.h>

#define	JEMALLOC_VERSION "3.6.0-200-g59cd80e6c6e36c26a880e86f6cde9f71808b256c"
#define	JEMALLOC_VERSION_MAJOR 3
#define	JEMALLOC_VERSION_MINOR 6
#define	JEMALLOC_VERSION_BUGFIX 0
#define	JEMALLOC_VERSION_NREV 200
#define	JEMALLOC_VERSION_GID "59cd80e6c6e36c26a880e86f6cde9f71808b256c"

#  define MALLOCX_LG_ALIGN(la)	(la)
#  if LG_SIZEOF_PTR == 2
#    define MALLOCX_ALIGN(a)	(ffs(a)-1)
#  else
#    define MALLOCX_ALIGN(a)						\
	 ((a < (size_t)INT_MAX) ? ffs(a)-1 : ffs(a>>32)+31)
#  endif
#  define MALLOCX_ZERO	((int)0x40)
/* Bias arena index bits so that 0 encodes "MALLOCX_ARENA() unspecified". */
#  define MALLOCX_ARENA(a)	((int)(((a)+1) << 8))

#ifdef JEMALLOC_HAVE_ATTR
#  define JEMALLOC_ATTR(s) __attribute__((s))
#  define JEMALLOC_EXPORT JEMALLOC_ATTR(visibility("default"))
#  define JEMALLOC_ALIGNED(s) JEMALLOC_ATTR(aligned(s))
#  define JEMALLOC_SECTION(s) JEMALLOC_ATTR(section(s))
#  define JEMALLOC_NOINLINE JEMALLOC_ATTR(noinline)
#elif _MSC_VER
#  define JEMALLOC_ATTR(s)
#  ifdef DLLEXPORT
#    define JEMALLOC_EXPORT __declspec(dllexport)
#  else
#    define JEMALLOC_EXPORT __declspec(dllimport)
#  endif
#  define JEMALLOC_ALIGNED(s) __declspec(align(s))
#  define JEMALLOC_SECTION(s) __declspec(allocate(s))
#  define JEMALLOC_NOINLINE __declspec(noinline)
#else
#  define JEMALLOC_ATTR(s)
#  define JEMALLOC_EXPORT
#  define JEMALLOC_ALIGNED(s)
#  define JEMALLOC_SECTION(s)
#  define JEMALLOC_NOINLINE
#endif

/*
 * The je_ prefix on the following public symbol declarations is an artifact
 * of namespace management, and should be omitted in application code unless
 * JEMALLOC_NO_DEMANGLE is defined (see jemalloc_mangle.h).
 */
extern JEMALLOC_EXPORT const char	*je_malloc_conf;
extern JEMALLOC_EXPORT void		(*je_malloc_message)(void *cbopaque,
    const char *s);

JEMALLOC_EXPORT void	*je_malloc(size_t size) JEMALLOC_ATTR(malloc);
JEMALLOC_EXPORT void	*je_calloc(size_t num, size_t size)
    JEMALLOC_ATTR(malloc);
JEMALLOC_EXPORT int	je_posix_memalign(void **memptr, size_t alignment,
    size_t size) JEMALLOC_ATTR(nonnull(1));
JEMALLOC_EXPORT void	*je_aligned_alloc(size_t alignment, size_t size)
    JEMALLOC_ATTR(malloc);
JEMALLOC_EXPORT void	*je_realloc(void *ptr, size_t size);
JEMALLOC_EXPORT void	je_free(void *ptr);

JEMALLOC_EXPORT void	*je_mallocx(size_t size, int flags)
    JEMALLOC_ATTR(malloc);
JEMALLOC_EXPORT void	*je_rallocx(void *ptr, size_t size, int flags);
JEMALLOC_EXPORT size_t	je_xallocx(void *ptr, size_t size, size_t extra,
    int flags);
JEMALLOC_EXPORT size_t	je_sallocx(const void *ptr, int flags)
    JEMALLOC_ATTR(pure);
JEMALLOC_EXPORT void	je_dallocx(void *ptr, int flags);
JEMALLOC_EXPORT void	je_sdallocx(void *ptr, size_t size, int flags);
JEMALLOC_EXPORT size_t	je_nallocx(size_t size, int flags)
    JEMALLOC_ATTR(pure);

JEMALLOC_EXPORT int	je_mallctl(const char *name, void *oldp,
    size_t *oldlenp, void *newp, size_t newlen);
JEMALLOC_EXPORT int	je_mallctlnametomib(const char *name, size_t *mibp,
    size_t *miblenp);
JEMALLOC_EXPORT int	je_mallctlbymib(const size_t *mib, size_t miblen,
    void *oldp, size_t *oldlenp, void *newp, size_t newlen);
JEMALLOC_EXPORT void	je_malloc_stats_print(void (*write_cb)(void *,
    const char *), void *je_cbopaque, const char *opts);
JEMALLOC_EXPORT size_t	je_malloc_usable_size(
    JEMALLOC_USABLE_SIZE_CONST void *ptr);

#ifdef JEMALLOC_OVERRIDE_MEMALIGN
JEMALLOC_EXPORT void *	je_memalign(size_t alignment, size_t size)
    JEMALLOC_ATTR(malloc);
#endif

#ifdef JEMALLOC_OVERRIDE_VALLOC
JEMALLOC_EXPORT void *	je_valloc(size_t size) JEMALLOC_ATTR(malloc);
#endif

typedef void *(chunk_alloc_t)(void *, size_t, size_t, bool *, unsigned);
typedef bool (chunk_dalloc_t)(void *, size_t, unsigned);

/*
 * By default application code must explicitly refer to mangled symbol names,
 * so that it is possible to use jemalloc in conjunction with another allocator
 * in the same application.  Define JEMALLOC_MANGLE in order to cause automatic
 * name mangling that matches the API prefixing that happened as a result of
 * --with-mangling and/or --with-jemalloc-prefix configuration settings.
 */
#ifdef JEMALLOC_MANGLE
#  ifndef JEMALLOC_NO_DEMANGLE
#    define JEMALLOC_NO_DEMANGLE
#  endif
#  define no je_no
#  define malloc_conf je_malloc_conf
#  define malloc_message je_malloc_message
#  define malloc je_malloc
#  define calloc je_calloc
#  define posix_memalign je_posix_memalign
#  define aligned_alloc je_aligned_alloc
#  define realloc je_realloc
#  define free je_free
#  define mallocx je_mallocx
#  define rallocx je_rallocx
#  define xallocx je_xallocx
#  define sallocx je_sallocx
#  define dallocx je_dallocx
#  define sdallocx je_sdallocx
#  define nallocx je_nallocx
#  define mallctl je_mallctl
#  define mallctlnametomib je_mallctlnametomib
#  define mallctlbymib je_mallctlbymib
#  define malloc_stats_print je_malloc_stats_print
#  define malloc_usable_size je_malloc_usable_size
#  define memalign je_memalign
#  define valloc je_valloc
#endif

/*
 * The je_* macros can be used as stable alternative names for the
 * public jemalloc API if JEMALLOC_NO_DEMANGLE is defined.  This is primarily
 * meant for use in jemalloc itself, but it can be used by application code to
 * provide isolation from the name mangling specified via --with-mangling
 * and/or --with-jemalloc-prefix.
 */
#ifndef JEMALLOC_NO_DEMANGLE
#  undef je_no
#  undef je_malloc_conf
#  undef je_malloc_message
#  undef je_malloc
#  undef je_calloc
#  undef je_posix_memalign
#  undef je_aligned_alloc
#  undef je_realloc
#  undef je_free
#  undef je_mallocx
#  undef je_rallocx
#  undef je_xallocx
#  undef je_sallocx
#  undef je_dallocx
#  undef je_sdallocx
#  undef je_nallocx
#  undef je_mallctl
#  undef je_mallctlnametomib
#  undef je_mallctlbymib
#  undef je_malloc_stats_print
#  undef je_malloc_usable_size
#  undef je_memalign
#  undef je_valloc
#endif

#ifdef __cplusplus
};
#endif
#endif /* JEMALLOC_H_ */
