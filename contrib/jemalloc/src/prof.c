#define	JEMALLOC_PROF_C_
#include "jemalloc/internal/jemalloc_internal.h"
/******************************************************************************/

#ifdef JEMALLOC_PROF_LIBUNWIND
#define	UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#ifdef JEMALLOC_PROF_LIBGCC
#include <unwind.h>
#endif

/******************************************************************************/
/* Data. */

bool		opt_prof = false;
bool		opt_prof_active = true;
size_t		opt_lg_prof_sample = LG_PROF_SAMPLE_DEFAULT;
ssize_t		opt_lg_prof_interval = LG_PROF_INTERVAL_DEFAULT;
bool		opt_prof_gdump = false;
bool		opt_prof_final = true;
bool		opt_prof_leak = false;
bool		opt_prof_accum = false;
char		opt_prof_prefix[
    /* Minimize memory bloat for non-prof builds. */
#ifdef JEMALLOC_PROF
    PATH_MAX +
#endif
    1];

uint64_t	prof_interval = 0;

size_t		lg_prof_sample;

/*
 * Table of mutexes that are shared among gctx's.  These are leaf locks, so
 * there is no problem with using them for more than one gctx at the same time.
 * The primary motivation for this sharing though is that gctx's are ephemeral,
 * and destroying mutexes causes complications for systems that allocate when
 * creating/destroying mutexes.
 */
static malloc_mutex_t	*gctx_locks;
static unsigned		cum_gctxs; /* Atomic counter. */

/*
 * Table of mutexes that are shared among tdata's.  No operations require
 * holding multiple tdata locks, so there is no problem with using them for more
 * than one tdata at the same time, even though a gctx lock may be acquired
 * while holding a tdata lock.
 */
static malloc_mutex_t	*tdata_locks;

/*
 * Global hash of (prof_bt_t *)-->(prof_gctx_t *).  This is the master data
 * structure that knows about all backtraces currently captured.
 */
static ckh_t		bt2gctx;
static malloc_mutex_t	bt2gctx_mtx;

/*
 * Tree of all extant prof_tdata_t structures, regardless of state,
 * {attached,detached,expired}.
 */
static prof_tdata_tree_t	tdatas;
static malloc_mutex_t	tdatas_mtx;

static uint64_t		next_thr_uid;
static malloc_mutex_t	next_thr_uid_mtx;

static malloc_mutex_t	prof_dump_seq_mtx;
static uint64_t		prof_dump_seq;
static uint64_t		prof_dump_iseq;
static uint64_t		prof_dump_mseq;
static uint64_t		prof_dump_useq;

/*
 * This buffer is rather large for stack allocation, so use a single buffer for
 * all profile dumps.
 */
static malloc_mutex_t	prof_dump_mtx;
static char		prof_dump_buf[
    /* Minimize memory bloat for non-prof builds. */
#ifdef JEMALLOC_PROF
    PROF_DUMP_BUFSIZE
#else
    1
#endif
];
static unsigned		prof_dump_buf_end;
static int		prof_dump_fd;

/* Do not dump any profiles until bootstrapping is complete. */
static bool		prof_booted = false;

/******************************************************************************/
/*
 * Function prototypes for static functions that are referenced prior to
 * definition.
 */

static bool	prof_tctx_should_destroy(prof_tctx_t *tctx);
static void	prof_tctx_destroy(tsd_t *tsd, prof_tctx_t *tctx);
static bool	prof_tdata_should_destroy(prof_tdata_t *tdata);
static void	prof_tdata_destroy(tsd_t *tsd, prof_tdata_t *tdata);

/******************************************************************************/
/* Red-black trees. */

JEMALLOC_INLINE_C int
prof_tctx_comp(const prof_tctx_t *a, const prof_tctx_t *b)
{
	uint64_t a_uid = a->tdata->thr_uid;
	uint64_t b_uid = b->tdata->thr_uid;

	return ((a_uid > b_uid) - (a_uid < b_uid));
}

rb_gen(static UNUSED, tctx_tree_, prof_tctx_tree_t, prof_tctx_t,
    tctx_link, prof_tctx_comp)

JEMALLOC_INLINE_C int
prof_gctx_comp(const prof_gctx_t *a, const prof_gctx_t *b)
{
	unsigned a_len = a->bt.len;
	unsigned b_len = b->bt.len;
	unsigned comp_len = (a_len < b_len) ? a_len : b_len;
	int ret = memcmp(a->bt.vec, b->bt.vec, comp_len * sizeof(void *));
	if (ret == 0)
		ret = (a_len > b_len) - (a_len < b_len);
	return (ret);
}

rb_gen(static UNUSED, gctx_tree_, prof_gctx_tree_t, prof_gctx_t, dump_link,
    prof_gctx_comp)

JEMALLOC_INLINE_C int
prof_tdata_comp(const prof_tdata_t *a, const prof_tdata_t *b)
{
	uint64_t a_uid = a->thr_uid;
	uint64_t b_uid = b->thr_uid;

	return ((a_uid > b_uid) - (a_uid < b_uid));
}

rb_gen(static UNUSED, tdata_tree_, prof_tdata_tree_t, prof_tdata_t, tdata_link,
    prof_tdata_comp)

/******************************************************************************/

void
prof_alloc_rollback(tsd_t *tsd, prof_tctx_t *tctx, bool updated)
{
	prof_tdata_t *tdata;

	cassert(config_prof);

	if (updated) {
		/*
		 * Compute a new sample threshold.  This isn't very important in
		 * practice, because this function is rarely executed, so the
		 * potential for sample bias is minimal except in contrived
		 * programs.
		 */
		tdata = prof_tdata_get(tsd, true);
		if (tdata != NULL)
			prof_sample_threshold_update(tctx->tdata);
	}

	if ((uintptr_t)tctx > (uintptr_t)1U) {
		malloc_mutex_lock(tctx->tdata->lock);
		tctx->prepared = false;
		if (prof_tctx_should_destroy(tctx))
			prof_tctx_destroy(tsd, tctx);
		else
			malloc_mutex_unlock(tctx->tdata->lock);
	}
}

void
prof_malloc_sample_object(const void *ptr, size_t usize, prof_tctx_t *tctx) {
	prof_tctx_set(ptr, tctx);

	malloc_mutex_lock(tctx->tdata->lock);
	tctx->cnts.curobjs++;
	tctx->cnts.curbytes += usize;
	if (opt_prof_accum) {
		tctx->cnts.accumobjs++;
		tctx->cnts.accumbytes += usize;
	}
	tctx->prepared = false;
	malloc_mutex_unlock(tctx->tdata->lock);
}

void
prof_free_sampled_object(tsd_t *tsd, size_t usize, prof_tctx_t *tctx)
{

	malloc_mutex_lock(tctx->tdata->lock);
	assert(tctx->cnts.curobjs > 0);
	assert(tctx->cnts.curbytes >= usize);
	tctx->cnts.curobjs--;
	tctx->cnts.curbytes -= usize;

	if (prof_tctx_should_destroy(tctx))
		prof_tctx_destroy(tsd, tctx);
	else
		malloc_mutex_unlock(tctx->tdata->lock);
}

void
bt_init(prof_bt_t *bt, void **vec)
{

	cassert(config_prof);

	bt->vec = vec;
	bt->len = 0;
}

static inline void
prof_enter(prof_tdata_t *tdata)
{

	cassert(config_prof);

	assert(tdata->enq == false);
	tdata->enq = true;

	malloc_mutex_lock(&bt2gctx_mtx);
}

static inline void
prof_leave(prof_tdata_t *tdata)
{
	bool idump, gdump;

	cassert(config_prof);

	malloc_mutex_unlock(&bt2gctx_mtx);

	assert(tdata->enq);
	tdata->enq = false;
	idump = tdata->enq_idump;
	tdata->enq_idump = false;
	gdump = tdata->enq_gdump;
	tdata->enq_gdump = false;

	if (idump)
		prof_idump();
	if (gdump)
		prof_gdump();
}

#ifdef JEMALLOC_PROF_LIBUNWIND
void
prof_backtrace(prof_bt_t *bt)
{
	int nframes;

	cassert(config_prof);
	assert(bt->len == 0);
	assert(bt->vec != NULL);

	nframes = unw_backtrace(bt->vec, PROF_BT_MAX);
	if (nframes <= 0)
		return;
	bt->len = nframes;
}
#elif (defined(JEMALLOC_PROF_LIBGCC))
static _Unwind_Reason_Code
prof_unwind_init_callback(struct _Unwind_Context *context, void *arg)
{

	cassert(config_prof);

	return (_URC_NO_REASON);
}

static _Unwind_Reason_Code
prof_unwind_callback(struct _Unwind_Context *context, void *arg)
{
	prof_unwind_data_t *data = (prof_unwind_data_t *)arg;
	void *ip;

	cassert(config_prof);

	ip = (void *)_Unwind_GetIP(context);
	if (ip == NULL)
		return (_URC_END_OF_STACK);
	data->bt->vec[data->bt->len] = ip;
	data->bt->len++;
	if (data->bt->len == data->max)
		return (_URC_END_OF_STACK);

	return (_URC_NO_REASON);
}

void
prof_backtrace(prof_bt_t *bt)
{
	prof_unwind_data_t data = {bt, PROF_BT_MAX};

	cassert(config_prof);

	_Unwind_Backtrace(prof_unwind_callback, &data);
}
#elif (defined(JEMALLOC_PROF_GCC))
void
prof_backtrace(prof_bt_t *bt)
{
#define	BT_FRAME(i)							\
	if ((i) < PROF_BT_MAX) {					\
		void *p;						\
		if (__builtin_frame_address(i) == 0)			\
			return;						\
		p = __builtin_return_address(i);			\
		if (p == NULL)						\
			return;						\
		bt->vec[(i)] = p;					\
		bt->len = (i) + 1;					\
	} else								\
		return;

	cassert(config_prof);

	BT_FRAME(0)
	BT_FRAME(1)
	BT_FRAME(2)
	BT_FRAME(3)
	BT_FRAME(4)
	BT_FRAME(5)
	BT_FRAME(6)
	BT_FRAME(7)
	BT_FRAME(8)
	BT_FRAME(9)

	BT_FRAME(10)
	BT_FRAME(11)
	BT_FRAME(12)
	BT_FRAME(13)
	BT_FRAME(14)
	BT_FRAME(15)
	BT_FRAME(16)
	BT_FRAME(17)
	BT_FRAME(18)
	BT_FRAME(19)

	BT_FRAME(20)
	BT_FRAME(21)
	BT_FRAME(22)
	BT_FRAME(23)
	BT_FRAME(24)
	BT_FRAME(25)
	BT_FRAME(26)
	BT_FRAME(27)
	BT_FRAME(28)
	BT_FRAME(29)

	BT_FRAME(30)
	BT_FRAME(31)
	BT_FRAME(32)
	BT_FRAME(33)
	BT_FRAME(34)
	BT_FRAME(35)
	BT_FRAME(36)
	BT_FRAME(37)
	BT_FRAME(38)
	BT_FRAME(39)

	BT_FRAME(40)
	BT_FRAME(41)
	BT_FRAME(42)
	BT_FRAME(43)
	BT_FRAME(44)
	BT_FRAME(45)
	BT_FRAME(46)
	BT_FRAME(47)
	BT_FRAME(48)
	BT_FRAME(49)

	BT_FRAME(50)
	BT_FRAME(51)
	BT_FRAME(52)
	BT_FRAME(53)
	BT_FRAME(54)
	BT_FRAME(55)
	BT_FRAME(56)
	BT_FRAME(57)
	BT_FRAME(58)
	BT_FRAME(59)

	BT_FRAME(60)
	BT_FRAME(61)
	BT_FRAME(62)
	BT_FRAME(63)
	BT_FRAME(64)
	BT_FRAME(65)
	BT_FRAME(66)
	BT_FRAME(67)
	BT_FRAME(68)
	BT_FRAME(69)

	BT_FRAME(70)
	BT_FRAME(71)
	BT_FRAME(72)
	BT_FRAME(73)
	BT_FRAME(74)
	BT_FRAME(75)
	BT_FRAME(76)
	BT_FRAME(77)
	BT_FRAME(78)
	BT_FRAME(79)

	BT_FRAME(80)
	BT_FRAME(81)
	BT_FRAME(82)
	BT_FRAME(83)
	BT_FRAME(84)
	BT_FRAME(85)
	BT_FRAME(86)
	BT_FRAME(87)
	BT_FRAME(88)
	BT_FRAME(89)

	BT_FRAME(90)
	BT_FRAME(91)
	BT_FRAME(92)
	BT_FRAME(93)
	BT_FRAME(94)
	BT_FRAME(95)
	BT_FRAME(96)
	BT_FRAME(97)
	BT_FRAME(98)
	BT_FRAME(99)

	BT_FRAME(100)
	BT_FRAME(101)
	BT_FRAME(102)
	BT_FRAME(103)
	BT_FRAME(104)
	BT_FRAME(105)
	BT_FRAME(106)
	BT_FRAME(107)
	BT_FRAME(108)
	BT_FRAME(109)

	BT_FRAME(110)
	BT_FRAME(111)
	BT_FRAME(112)
	BT_FRAME(113)
	BT_FRAME(114)
	BT_FRAME(115)
	BT_FRAME(116)
	BT_FRAME(117)
	BT_FRAME(118)
	BT_FRAME(119)

	BT_FRAME(120)
	BT_FRAME(121)
	BT_FRAME(122)
	BT_FRAME(123)
	BT_FRAME(124)
	BT_FRAME(125)
	BT_FRAME(126)
	BT_FRAME(127)
#undef BT_FRAME
}
#else
void
prof_backtrace(prof_bt_t *bt)
{

	cassert(config_prof);
	not_reached();
}
#endif

static malloc_mutex_t *
prof_gctx_mutex_choose(void)
{
	unsigned ngctxs = atomic_add_u(&cum_gctxs, 1);

	return (&gctx_locks[(ngctxs - 1) % PROF_NCTX_LOCKS]);
}

static malloc_mutex_t *
prof_tdata_mutex_choose(uint64_t thr_uid)
{

	return (&tdata_locks[thr_uid % PROF_NTDATA_LOCKS]);
}

static prof_gctx_t *
prof_gctx_create(tsd_t *tsd, prof_bt_t *bt)
{
	/*
	 * Create a single allocation that has space for vec of length bt->len.
	 */
	prof_gctx_t *gctx = (prof_gctx_t *)imalloc(tsd, offsetof(prof_gctx_t,
	    vec) + (bt->len * sizeof(void *)));
	if (gctx == NULL)
		return (NULL);
	gctx->lock = prof_gctx_mutex_choose();
	/*
	 * Set nlimbo to 1, in order to avoid a race condition with
	 * prof_tctx_destroy()/prof_gctx_maybe_destroy().
	 */
	gctx->nlimbo = 1;
	tctx_tree_new(&gctx->tctxs);
	/* Duplicate bt. */
	memcpy(gctx->vec, bt->vec, bt->len * sizeof(void *));
	gctx->bt.vec = gctx->vec;
	gctx->bt.len = bt->len;
	return (gctx);
}

static void
prof_gctx_maybe_destroy(tsd_t *tsd, prof_gctx_t *gctx, prof_tdata_t *tdata)
{

	cassert(config_prof);

	/*
	 * Check that gctx is still unused by any thread cache before destroying
	 * it.  prof_lookup() increments gctx->nlimbo in order to avoid a race
	 * condition with this function, as does prof_tctx_destroy() in order to
	 * avoid a race between the main body of prof_tctx_destroy() and entry
	 * into this function.
	 */
	prof_enter(tdata);
	malloc_mutex_lock(gctx->lock);
	if (tctx_tree_empty(&gctx->tctxs) && gctx->nlimbo == 1) {
		/* Remove gctx from bt2gctx. */
		if (ckh_remove(tsd, &bt2gctx, &gctx->bt, NULL, NULL))
			not_reached();
		prof_leave(tdata);
		/* Destroy gctx. */
		malloc_mutex_unlock(gctx->lock);
		idalloc(tsd, gctx);
	} else {
		/*
		 * Compensate for increment in prof_tctx_destroy() or
		 * prof_lookup().
		 */
		gctx->nlimbo--;
		malloc_mutex_unlock(gctx->lock);
		prof_leave(tdata);
	}
}

/* tctx->tdata->lock must be held. */
static bool
prof_tctx_should_destroy(prof_tctx_t *tctx)
{

	if (opt_prof_accum)
		return (false);
	if (tctx->cnts.curobjs != 0)
		return (false);
	if (tctx->prepared)
		return (false);
	return (true);
}

static bool
prof_gctx_should_destroy(prof_gctx_t *gctx)
{

	if (opt_prof_accum)
		return (false);
	if (tctx_tree_empty(&gctx->tctxs) == false)
		return (false);
	if (gctx->nlimbo != 0)
		return (false);
	return (true);
}

/* tctx->tdata->lock is held upon entry, and released before return. */
static void
prof_tctx_destroy(tsd_t *tsd, prof_tctx_t *tctx)
{
	prof_tdata_t *tdata = tctx->tdata;
	prof_gctx_t *gctx = tctx->gctx;
	bool destroy_tdata, destroy_gctx;

	assert(tctx->cnts.curobjs == 0);
	assert(tctx->cnts.curbytes == 0);
	assert(opt_prof_accum == false);
	assert(tctx->cnts.accumobjs == 0);
	assert(tctx->cnts.accumbytes == 0);

	ckh_remove(tsd, &tdata->bt2tctx, &gctx->bt, NULL, NULL);
	destroy_tdata = prof_tdata_should_destroy(tdata);
	malloc_mutex_unlock(tdata->lock);

	malloc_mutex_lock(gctx->lock);
	tctx_tree_remove(&gctx->tctxs, tctx);
	if (prof_gctx_should_destroy(gctx)) {
		/*
		 * Increment gctx->nlimbo in order to keep another thread from
		 * winning the race to destroy gctx while this one has
		 * gctx->lock dropped.  Without this, it would be possible for
		 * another thread to:
		 *
		 * 1) Sample an allocation associated with gctx.
		 * 2) Deallocate the sampled object.
		 * 3) Successfully prof_gctx_maybe_destroy(gctx).
		 *
		 * The result would be that gctx no longer exists by the time
		 * this thread accesses it in prof_gctx_maybe_destroy().
		 */
		gctx->nlimbo++;
		destroy_gctx = true;
	} else
		destroy_gctx = false;
	malloc_mutex_unlock(gctx->lock);
	if (destroy_gctx)
		prof_gctx_maybe_destroy(tsd, gctx, tdata);

	if (destroy_tdata)
		prof_tdata_destroy(tsd, tdata);

	idalloc(tsd, tctx);
}

static bool
prof_lookup_global(tsd_t *tsd, prof_bt_t *bt, prof_tdata_t *tdata,
    void **p_btkey, prof_gctx_t **p_gctx, bool *p_new_gctx)
{
	union {
		prof_gctx_t	*p;
		void		*v;
	} gctx;
	union {
		prof_bt_t	*p;
		void		*v;
	} btkey;
	bool new_gctx;

	prof_enter(tdata);
	if (ckh_search(&bt2gctx, bt, &btkey.v, &gctx.v)) {
		/* bt has never been seen before.  Insert it. */
		gctx.p = prof_gctx_create(tsd, bt);
		if (gctx.v == NULL) {
			prof_leave(tdata);
			return (true);
		}
		btkey.p = &gctx.p->bt;
		if (ckh_insert(tsd, &bt2gctx, btkey.v, gctx.v)) {
			/* OOM. */
			prof_leave(tdata);
			idalloc(tsd, gctx.v);
			return (true);
		}
		new_gctx = true;
	} else {
		/*
		 * Increment nlimbo, in order to avoid a race condition with
		 * prof_tctx_destroy()/prof_gctx_maybe_destroy().
		 */
		malloc_mutex_lock(gctx.p->lock);
		gctx.p->nlimbo++;
		malloc_mutex_unlock(gctx.p->lock);
		new_gctx = false;
	}
	prof_leave(tdata);

	*p_btkey = btkey.v;
	*p_gctx = gctx.p;
	*p_new_gctx = new_gctx;
	return (false);
}

prof_tctx_t *
prof_lookup(tsd_t *tsd, prof_bt_t *bt)
{
	union {
		prof_tctx_t	*p;
		void		*v;
	} ret;
	prof_tdata_t *tdata;
	bool not_found;

	cassert(config_prof);

	tdata = prof_tdata_get(tsd, false);
	if (tdata == NULL)
		return (NULL);

	malloc_mutex_lock(tdata->lock);
	not_found = ckh_search(&tdata->bt2tctx, bt, NULL, &ret.v);
	if (!not_found) /* Note double negative! */
		ret.p->prepared = true;
	malloc_mutex_unlock(tdata->lock);
	if (not_found) {
		void *btkey;
		prof_gctx_t *gctx;
		bool new_gctx, error;

		/*
		 * This thread's cache lacks bt.  Look for it in the global
		 * cache.
		 */
		if (prof_lookup_global(tsd, bt, tdata, &btkey, &gctx,
		    &new_gctx))
			return (NULL);

		/* Link a prof_tctx_t into gctx for this thread. */
		ret.v = imalloc(tsd, sizeof(prof_tctx_t));
		if (ret.p == NULL) {
			if (new_gctx)
				prof_gctx_maybe_destroy(tsd, gctx, tdata);
			return (NULL);
		}
		ret.p->tdata = tdata;
		memset(&ret.p->cnts, 0, sizeof(prof_cnt_t));
		ret.p->gctx = gctx;
		ret.p->prepared = true;
		ret.p->state = prof_tctx_state_initializing;
		malloc_mutex_lock(tdata->lock);
		error = ckh_insert(tsd, &tdata->bt2tctx, btkey, ret.v);
		malloc_mutex_unlock(tdata->lock);
		if (error) {
			if (new_gctx)
				prof_gctx_maybe_destroy(tsd, gctx, tdata);
			idalloc(tsd, ret.v);
			return (NULL);
		}
		malloc_mutex_lock(gctx->lock);
		ret.p->state = prof_tctx_state_nominal;
		tctx_tree_insert(&gctx->tctxs, ret.p);
		gctx->nlimbo--;
		malloc_mutex_unlock(gctx->lock);
	}

	return (ret.p);
}

void
prof_sample_threshold_update(prof_tdata_t *tdata)
{
	/*
	 * The body of this function is compiled out unless heap profiling is
	 * enabled, so that it is possible to compile jemalloc with floating
	 * point support completely disabled.  Avoiding floating point code is
	 * important on memory-constrained systems, but it also enables a
	 * workaround for versions of glibc that don't properly save/restore
	 * floating point registers during dynamic lazy symbol loading (which
	 * internally calls into whatever malloc implementation happens to be
	 * integrated into the application).  Note that some compilers (e.g.
	 * gcc 4.8) may use floating point registers for fast memory moves, so
	 * jemalloc must be compiled with such optimizations disabled (e.g.
	 * -mno-sse) in order for the workaround to be complete.
	 */
#ifdef JEMALLOC_PROF
	uint64_t r;
	double u;

	if (!config_prof)
		return;

	if (lg_prof_sample == 0) {
		tdata->bytes_until_sample = 0;
		return;
	}

	/*
	 * Compute sample interval as a geometrically distributed random
	 * variable with mean (2^lg_prof_sample).
	 *
	 *                             __        __
	 *                             |  log(u)  |                     1
	 * tdata->bytes_until_sample = | -------- |, where p = ---------------
	 *                             | log(1-p) |             lg_prof_sample
	 *                                                     2
	 *
	 * For more information on the math, see:
	 *
	 *   Non-Uniform Random Variate Generation
	 *   Luc Devroye
	 *   Springer-Verlag, New York, 1986
	 *   pp 500
	 *   (http://luc.devroye.org/rnbookindex.html)
	 */
	prng64(r, 53, tdata->prng_state, UINT64_C(6364136223846793005),
	    UINT64_C(1442695040888963407));
	u = (double)r * (1.0/9007199254740992.0L);
	tdata->bytes_until_sample = (uint64_t)(log(u) /
	    log(1.0 - (1.0 / (double)((uint64_t)1U << lg_prof_sample))))
	    + (uint64_t)1U;
#endif
}

#ifdef JEMALLOC_JET
size_t
prof_bt_count(void)
{
	size_t bt_count;
	tsd_t *tsd;
	prof_tdata_t *tdata;

	if ((tsd = tsd_tryget()) == NULL)
		return (0);
	tdata = prof_tdata_get(tsd, false);
	if (tdata == NULL)
		return (0);

	prof_enter(tdata);
	bt_count = ckh_count(&bt2gctx);
	prof_leave(tdata);

	return (bt_count);
}
#endif

#ifdef JEMALLOC_JET
#undef prof_dump_open
#define	prof_dump_open JEMALLOC_N(prof_dump_open_impl)
#endif
static int
prof_dump_open(bool propagate_err, const char *filename)
{
	int fd;

	fd = creat(filename, 0644);
	if (fd == -1 && propagate_err == false) {
		malloc_printf("<jemalloc>: creat(\"%s\"), 0644) failed\n",
		    filename);
		if (opt_abort)
			abort();
	}

	return (fd);
}
#ifdef JEMALLOC_JET
#undef prof_dump_open
#define	prof_dump_open JEMALLOC_N(prof_dump_open)
prof_dump_open_t *prof_dump_open = JEMALLOC_N(prof_dump_open_impl);
#endif

static bool
prof_dump_flush(bool propagate_err)
{
	bool ret = false;
	ssize_t err;

	cassert(config_prof);

	err = write(prof_dump_fd, prof_dump_buf, prof_dump_buf_end);
	if (err == -1) {
		if (propagate_err == false) {
			malloc_write("<jemalloc>: write() failed during heap "
			    "profile flush\n");
			if (opt_abort)
				abort();
		}
		ret = true;
	}
	prof_dump_buf_end = 0;

	return (ret);
}

static bool
prof_dump_close(bool propagate_err)
{
	bool ret;

	assert(prof_dump_fd != -1);
	ret = prof_dump_flush(propagate_err);
	close(prof_dump_fd);
	prof_dump_fd = -1;

	return (ret);
}

static bool
prof_dump_write(bool propagate_err, const char *s)
{
	unsigned i, slen, n;

	cassert(config_prof);

	i = 0;
	slen = strlen(s);
	while (i < slen) {
		/* Flush the buffer if it is full. */
		if (prof_dump_buf_end == PROF_DUMP_BUFSIZE)
			if (prof_dump_flush(propagate_err) && propagate_err)
				return (true);

		if (prof_dump_buf_end + slen <= PROF_DUMP_BUFSIZE) {
			/* Finish writing. */
			n = slen - i;
		} else {
			/* Write as much of s as will fit. */
			n = PROF_DUMP_BUFSIZE - prof_dump_buf_end;
		}
		memcpy(&prof_dump_buf[prof_dump_buf_end], &s[i], n);
		prof_dump_buf_end += n;
		i += n;
	}

	return (false);
}

JEMALLOC_ATTR(format(printf, 2, 3))
static bool
prof_dump_printf(bool propagate_err, const char *format, ...)
{
	bool ret;
	va_list ap;
	char buf[PROF_PRINTF_BUFSIZE];

	va_start(ap, format);
	malloc_vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	ret = prof_dump_write(propagate_err, buf);

	return (ret);
}

/* tctx->tdata->lock is held. */
static void
prof_tctx_merge_tdata(prof_tctx_t *tctx, prof_tdata_t *tdata)
{

	malloc_mutex_lock(tctx->gctx->lock);
	if (tctx->state == prof_tctx_state_initializing) {
		malloc_mutex_unlock(tctx->gctx->lock);
		return;
	}
	assert(tctx->state == prof_tctx_state_nominal);
	tctx->state = prof_tctx_state_dumping;
	malloc_mutex_unlock(tctx->gctx->lock);

	memcpy(&tctx->dump_cnts, &tctx->cnts, sizeof(prof_cnt_t));

	tdata->cnt_summed.curobjs += tctx->dump_cnts.curobjs;
	tdata->cnt_summed.curbytes += tctx->dump_cnts.curbytes;
	if (opt_prof_accum) {
		tdata->cnt_summed.accumobjs += tctx->dump_cnts.accumobjs;
		tdata->cnt_summed.accumbytes += tctx->dump_cnts.accumbytes;
	}
}

/* gctx->lock is held. */
static void
prof_tctx_merge_gctx(prof_tctx_t *tctx, prof_gctx_t *gctx)
{

	gctx->cnt_summed.curobjs += tctx->dump_cnts.curobjs;
	gctx->cnt_summed.curbytes += tctx->dump_cnts.curbytes;
	if (opt_prof_accum) {
		gctx->cnt_summed.accumobjs += tctx->dump_cnts.accumobjs;
		gctx->cnt_summed.accumbytes += tctx->dump_cnts.accumbytes;
	}
}

/* tctx->gctx is held. */
static prof_tctx_t *
prof_tctx_merge_iter(prof_tctx_tree_t *tctxs, prof_tctx_t *tctx, void *arg)
{

	switch (tctx->state) {
	case prof_tctx_state_nominal:
		/* New since dumping started; ignore. */
		break;
	case prof_tctx_state_dumping:
	case prof_tctx_state_purgatory:
		prof_tctx_merge_gctx(tctx, tctx->gctx);
		break;
	default:
		not_reached();
	}

	return (NULL);
}

/* gctx->lock is held. */
static prof_tctx_t *
prof_tctx_dump_iter(prof_tctx_tree_t *tctxs, prof_tctx_t *tctx, void *arg)
{
	bool propagate_err = *(bool *)arg;

	if (prof_dump_printf(propagate_err,
	    "  t%"PRIu64": %"PRIu64": %"PRIu64" [%"PRIu64": %"PRIu64"]\n",
	    tctx->tdata->thr_uid, tctx->dump_cnts.curobjs,
	    tctx->dump_cnts.curbytes, tctx->dump_cnts.accumobjs,
	    tctx->dump_cnts.accumbytes))
		return (tctx);
	return (NULL);
}

/* tctx->gctx is held. */
static prof_tctx_t *
prof_tctx_finish_iter(prof_tctx_tree_t *tctxs, prof_tctx_t *tctx, void *arg)
{
	prof_tctx_t *ret;
	tsd_t *tsd = (tsd_t *)arg;

	switch (tctx->state) {
	case prof_tctx_state_nominal:
		/* New since dumping started; ignore. */
		break;
	case prof_tctx_state_dumping:
		tctx->state = prof_tctx_state_nominal;
		break;
	case prof_tctx_state_purgatory:
		ret = tctx_tree_next(tctxs, tctx);
		tctx_tree_remove(tctxs, tctx);
		idalloc(tsd, tctx);
		goto label_return;
	default:
		not_reached();
	}

	ret = NULL;
label_return:
	return (ret);
}

static void
prof_dump_gctx_prep(prof_gctx_t *gctx, prof_gctx_tree_t *gctxs)
{

	cassert(config_prof);

	malloc_mutex_lock(gctx->lock);

	/*
	 * Increment nlimbo so that gctx won't go away before dump.
	 * Additionally, link gctx into the dump list so that it is included in
	 * prof_dump()'s second pass.
	 */
	gctx->nlimbo++;
	gctx_tree_insert(gctxs, gctx);

	memset(&gctx->cnt_summed, 0, sizeof(prof_cnt_t));

	malloc_mutex_unlock(gctx->lock);
}

static prof_gctx_t *
prof_gctx_merge_iter(prof_gctx_tree_t *gctxs, prof_gctx_t *gctx, void *arg)
{
	size_t *leak_ngctx = (size_t *)arg;

	malloc_mutex_lock(gctx->lock);
	tctx_tree_iter(&gctx->tctxs, NULL, prof_tctx_merge_iter, NULL);
	if (gctx->cnt_summed.curobjs != 0)
		(*leak_ngctx)++;
	malloc_mutex_unlock(gctx->lock);

	return (NULL);
}

static prof_gctx_t *
prof_gctx_finish_iter(prof_gctx_tree_t *gctxs, prof_gctx_t *gctx, void *arg)
{
	tsd_t *tsd = (tsd_t *)arg;
	prof_tdata_t *tdata = prof_tdata_get(tsd, false);
	prof_tctx_t *next;
	bool destroy_gctx;

	malloc_mutex_lock(gctx->lock);
	next = NULL;
	do {
		next = tctx_tree_iter(&gctx->tctxs, next, prof_tctx_finish_iter,
		    tsd);
	} while (next != NULL);
	gctx->nlimbo--;
	destroy_gctx = prof_gctx_should_destroy(gctx);
	malloc_mutex_unlock(gctx->lock);
	if (destroy_gctx)
		prof_gctx_maybe_destroy(tsd, gctx, tdata);

	return (NULL);
}

static prof_tdata_t *
prof_tdata_merge_iter(prof_tdata_tree_t *tdatas, prof_tdata_t *tdata, void *arg)
{
	prof_cnt_t *cnt_all = (prof_cnt_t *)arg;

	malloc_mutex_lock(tdata->lock);
	if (tdata->state != prof_tdata_state_expired) {
		size_t tabind;
		union {
			prof_tctx_t	*p;
			void		*v;
		} tctx;

		tdata->dumping = true;
		memset(&tdata->cnt_summed, 0, sizeof(prof_cnt_t));
		for (tabind = 0; ckh_iter(&tdata->bt2tctx, &tabind, NULL,
		    &tctx.v) == false;)
			prof_tctx_merge_tdata(tctx.p, tdata);

		cnt_all->curobjs += tdata->cnt_summed.curobjs;
		cnt_all->curbytes += tdata->cnt_summed.curbytes;
		if (opt_prof_accum) {
			cnt_all->accumobjs += tdata->cnt_summed.accumobjs;
			cnt_all->accumbytes += tdata->cnt_summed.accumbytes;
		}
	} else
		tdata->dumping = false;
	malloc_mutex_unlock(tdata->lock);

	return (NULL);
}

static prof_tdata_t *
prof_tdata_dump_iter(prof_tdata_tree_t *tdatas, prof_tdata_t *tdata, void *arg)
{
	bool propagate_err = *(bool *)arg;

	if (tdata->dumping == false)
		return (NULL);

	if (prof_dump_printf(propagate_err,
	    "  t%"PRIu64": %"PRIu64": %"PRIu64" [%"PRIu64": %"PRIu64"]%s%s\n",
	    tdata->thr_uid, tdata->cnt_summed.curobjs,
	    tdata->cnt_summed.curbytes, tdata->cnt_summed.accumobjs,
	    tdata->cnt_summed.accumbytes,
	    (tdata->thread_name != NULL) ? " " : "",
	    (tdata->thread_name != NULL) ? tdata->thread_name : ""))
		return (tdata);
	return (NULL);
}

static bool
prof_dump_header(bool propagate_err, const prof_cnt_t *cnt_all)
{
	bool ret;

	if (prof_dump_printf(propagate_err,
	    "heap_v2/%"PRIu64"\n"
	    "  t*: %"PRIu64": %"PRIu64" [%"PRIu64": %"PRIu64"]\n",
	    ((uint64_t)1U << lg_prof_sample), cnt_all->curobjs,
	    cnt_all->curbytes, cnt_all->accumobjs, cnt_all->accumbytes))
		return (true);

	malloc_mutex_lock(&tdatas_mtx);
	ret = (tdata_tree_iter(&tdatas, NULL, prof_tdata_dump_iter,
	    (void *)&propagate_err) != NULL);
	malloc_mutex_unlock(&tdatas_mtx);
	return (ret);
}

/* gctx->lock is held. */
static bool
prof_dump_gctx(bool propagate_err, prof_gctx_t *gctx, const prof_bt_t *bt,
    prof_gctx_tree_t *gctxs)
{
	bool ret;
	unsigned i;

	cassert(config_prof);

	/* Avoid dumping such gctx's that have no useful data. */
	if ((opt_prof_accum == false && gctx->cnt_summed.curobjs == 0) ||
	    (opt_prof_accum && gctx->cnt_summed.accumobjs == 0)) {
		assert(gctx->cnt_summed.curobjs == 0);
		assert(gctx->cnt_summed.curbytes == 0);
		assert(gctx->cnt_summed.accumobjs == 0);
		assert(gctx->cnt_summed.accumbytes == 0);
		ret = false;
		goto label_return;
	}

	if (prof_dump_printf(propagate_err, "@")) {
		ret = true;
		goto label_return;
	}
	for (i = 0; i < bt->len; i++) {
		if (prof_dump_printf(propagate_err, " %#"PRIxPTR,
		    (uintptr_t)bt->vec[i])) {
			ret = true;
			goto label_return;
		}
	}

	if (prof_dump_printf(propagate_err,
	    "\n"
	    "  t*: %"PRIu64": %"PRIu64" [%"PRIu64": %"PRIu64"]\n",
	    gctx->cnt_summed.curobjs, gctx->cnt_summed.curbytes,
	    gctx->cnt_summed.accumobjs, gctx->cnt_summed.accumbytes)) {
		ret = true;
		goto label_return;
	}

	if (tctx_tree_iter(&gctx->tctxs, NULL, prof_tctx_dump_iter,
	    (void *)&propagate_err) != NULL) {
		ret = true;
		goto label_return;
	}

	ret = false;
label_return:
	return (ret);
}

static bool
prof_dump_maps(bool propagate_err)
{
	bool ret;
	int mfd;
	char filename[PATH_MAX + 1];

	cassert(config_prof);
#ifdef __FreeBSD__
	malloc_snprintf(filename, sizeof(filename), "/proc/curproc/map");
#else
	malloc_snprintf(filename, sizeof(filename), "/proc/%d/maps",
	    (int)getpid());
#endif
	mfd = open(filename, O_RDONLY);
	if (mfd != -1) {
		ssize_t nread;

		if (prof_dump_write(propagate_err, "\nMAPPED_LIBRARIES:\n") &&
		    propagate_err) {
			ret = true;
			goto label_return;
		}
		nread = 0;
		do {
			prof_dump_buf_end += nread;
			if (prof_dump_buf_end == PROF_DUMP_BUFSIZE) {
				/* Make space in prof_dump_buf before read(). */
				if (prof_dump_flush(propagate_err) &&
				    propagate_err) {
					ret = true;
					goto label_return;
				}
			}
			nread = read(mfd, &prof_dump_buf[prof_dump_buf_end],
			    PROF_DUMP_BUFSIZE - prof_dump_buf_end);
		} while (nread > 0);
	} else {
		ret = true;
		goto label_return;
	}

	ret = false;
label_return:
	if (mfd != -1)
		close(mfd);
	return (ret);
}

static void
prof_leakcheck(const prof_cnt_t *cnt_all, size_t leak_ngctx,
    const char *filename)
{

	if (cnt_all->curbytes != 0) {
		malloc_printf("<jemalloc>: Leak summary: %"PRIu64" byte%s, %"
		    PRIu64" object%s, %zu context%s\n",
		    cnt_all->curbytes, (cnt_all->curbytes != 1) ? "s" : "",
		    cnt_all->curobjs, (cnt_all->curobjs != 1) ? "s" : "",
		    leak_ngctx, (leak_ngctx != 1) ? "s" : "");
		malloc_printf(
		    "<jemalloc>: Run pprof on \"%s\" for leak detail\n",
		    filename);
	}
}

static prof_gctx_t *
prof_gctx_dump_iter(prof_gctx_tree_t *gctxs, prof_gctx_t *gctx, void *arg)
{
	prof_gctx_t *ret;
	bool propagate_err = *(bool *)arg;

	malloc_mutex_lock(gctx->lock);

	if (prof_dump_gctx(propagate_err, gctx, &gctx->bt, gctxs)) {
		ret = gctx_tree_next(gctxs, gctx);
		goto label_return;
	}

	ret = NULL;
label_return:
	malloc_mutex_unlock(gctx->lock);
	return (ret);
}

static bool
prof_dump(tsd_t *tsd, bool propagate_err, const char *filename, bool leakcheck)
{
	prof_tdata_t *tdata;
	prof_cnt_t cnt_all;
	size_t tabind;
	union {
		prof_gctx_t	*p;
		void		*v;
	} gctx;
	size_t leak_ngctx;
	prof_gctx_tree_t gctxs;

	cassert(config_prof);

	tdata = prof_tdata_get(tsd, false);
	if (tdata == NULL)
		return (true);

	malloc_mutex_lock(&prof_dump_mtx);
	prof_enter(tdata);

	/*
	 * Put gctx's in limbo and clear their counters in preparation for
	 * summing.
	 */
	gctx_tree_new(&gctxs);
	for (tabind = 0; ckh_iter(&bt2gctx, &tabind, NULL, &gctx.v) == false;)
		prof_dump_gctx_prep(gctx.p, &gctxs);

	/*
	 * Iterate over tdatas, and for the non-expired ones snapshot their tctx
	 * stats and merge them into the associated gctx's.
	 */
	memset(&cnt_all, 0, sizeof(prof_cnt_t));
	malloc_mutex_lock(&tdatas_mtx);
	tdata_tree_iter(&tdatas, NULL, prof_tdata_merge_iter, (void *)&cnt_all);
	malloc_mutex_unlock(&tdatas_mtx);

	/* Merge tctx stats into gctx's. */
	leak_ngctx = 0;
	gctx_tree_iter(&gctxs, NULL, prof_gctx_merge_iter, (void *)&leak_ngctx);

	prof_leave(tdata);

	/* Create dump file. */
	if ((prof_dump_fd = prof_dump_open(propagate_err, filename)) == -1)
		goto label_open_close_error;

	/* Dump profile header. */
	if (prof_dump_header(propagate_err, &cnt_all))
		goto label_write_error;

	/* Dump per gctx profile stats. */
	if (gctx_tree_iter(&gctxs, NULL, prof_gctx_dump_iter,
	    (void *)&propagate_err) != NULL)
		goto label_write_error;

	/* Dump /proc/<pid>/maps if possible. */
	if (prof_dump_maps(propagate_err))
		goto label_write_error;

	if (prof_dump_close(propagate_err))
		goto label_open_close_error;

	gctx_tree_iter(&gctxs, NULL, prof_gctx_finish_iter, tsd);
	malloc_mutex_unlock(&prof_dump_mtx);

	if (leakcheck)
		prof_leakcheck(&cnt_all, leak_ngctx, filename);

	return (false);
label_write_error:
	prof_dump_close(propagate_err);
label_open_close_error:
	gctx_tree_iter(&gctxs, NULL, prof_gctx_finish_iter, tsd);
	malloc_mutex_unlock(&prof_dump_mtx);
	return (true);
}

#define	DUMP_FILENAME_BUFSIZE	(PATH_MAX + 1)
#define	VSEQ_INVALID		UINT64_C(0xffffffffffffffff)
static void
prof_dump_filename(char *filename, char v, uint64_t vseq)
{

	cassert(config_prof);

	if (vseq != VSEQ_INVALID) {
	        /* "<prefix>.<pid>.<seq>.v<vseq>.heap" */
		malloc_snprintf(filename, DUMP_FILENAME_BUFSIZE,
		    "%s.%d.%"PRIu64".%c%"PRIu64".heap",
		    opt_prof_prefix, (int)getpid(), prof_dump_seq, v, vseq);
	} else {
	        /* "<prefix>.<pid>.<seq>.<v>.heap" */
		malloc_snprintf(filename, DUMP_FILENAME_BUFSIZE,
		    "%s.%d.%"PRIu64".%c.heap",
		    opt_prof_prefix, (int)getpid(), prof_dump_seq, v);
	}
	prof_dump_seq++;
}

static void
prof_fdump(void)
{
	tsd_t *tsd;
	char filename[DUMP_FILENAME_BUFSIZE];

	cassert(config_prof);

	if (prof_booted == false)
		return;
	if ((tsd = tsd_tryget()) == NULL)
		return;

	if (opt_prof_final && opt_prof_prefix[0] != '\0') {
		malloc_mutex_lock(&prof_dump_seq_mtx);
		prof_dump_filename(filename, 'f', VSEQ_INVALID);
		malloc_mutex_unlock(&prof_dump_seq_mtx);
		prof_dump(tsd, false, filename, opt_prof_leak);
	}
}

void
prof_idump(void)
{
	tsd_t *tsd;
	prof_tdata_t *tdata;
	char filename[PATH_MAX + 1];

	cassert(config_prof);

	if (prof_booted == false)
		return;
	if ((tsd = tsd_tryget()) == NULL)
		return;
	tdata = prof_tdata_get(tsd, false);
	if (tdata == NULL)
		return;
	if (tdata->enq) {
		tdata->enq_idump = true;
		return;
	}

	if (opt_prof_prefix[0] != '\0') {
		malloc_mutex_lock(&prof_dump_seq_mtx);
		prof_dump_filename(filename, 'i', prof_dump_iseq);
		prof_dump_iseq++;
		malloc_mutex_unlock(&prof_dump_seq_mtx);
		prof_dump(tsd, false, filename, false);
	}
}

bool
prof_mdump(const char *filename)
{
	tsd_t *tsd;
	char filename_buf[DUMP_FILENAME_BUFSIZE];

	cassert(config_prof);

	if (opt_prof == false || prof_booted == false)
		return (true);
	if ((tsd = tsd_tryget()) == NULL)
		return (true);

	if (filename == NULL) {
		/* No filename specified, so automatically generate one. */
		if (opt_prof_prefix[0] == '\0')
			return (true);
		malloc_mutex_lock(&prof_dump_seq_mtx);
		prof_dump_filename(filename_buf, 'm', prof_dump_mseq);
		prof_dump_mseq++;
		malloc_mutex_unlock(&prof_dump_seq_mtx);
		filename = filename_buf;
	}
	return (prof_dump(tsd, true, filename, false));
}

void
prof_gdump(void)
{
	tsd_t *tsd;
	prof_tdata_t *tdata;
	char filename[DUMP_FILENAME_BUFSIZE];

	cassert(config_prof);

	if (prof_booted == false)
		return;
	if ((tsd = tsd_tryget()) == NULL)
		return;
	tdata = prof_tdata_get(tsd, false);
	if (tdata == NULL)
		return;
	if (tdata->enq) {
		tdata->enq_gdump = true;
		return;
	}

	if (opt_prof_prefix[0] != '\0') {
		malloc_mutex_lock(&prof_dump_seq_mtx);
		prof_dump_filename(filename, 'u', prof_dump_useq);
		prof_dump_useq++;
		malloc_mutex_unlock(&prof_dump_seq_mtx);
		prof_dump(tsd, false, filename, false);
	}
}

static void
prof_bt_hash(const void *key, size_t r_hash[2])
{
	prof_bt_t *bt = (prof_bt_t *)key;

	cassert(config_prof);

	hash(bt->vec, bt->len * sizeof(void *), 0x94122f33U, r_hash);
}

static bool
prof_bt_keycomp(const void *k1, const void *k2)
{
	const prof_bt_t *bt1 = (prof_bt_t *)k1;
	const prof_bt_t *bt2 = (prof_bt_t *)k2;

	cassert(config_prof);

	if (bt1->len != bt2->len)
		return (false);
	return (memcmp(bt1->vec, bt2->vec, bt1->len * sizeof(void *)) == 0);
}

JEMALLOC_INLINE_C uint64_t
prof_thr_uid_alloc(void)
{
	uint64_t thr_uid;

	malloc_mutex_lock(&next_thr_uid_mtx);
	thr_uid = next_thr_uid;
	next_thr_uid++;
	malloc_mutex_unlock(&next_thr_uid_mtx);

	return (thr_uid);
}

static prof_tdata_t *
prof_tdata_init_impl(tsd_t *tsd, uint64_t thr_uid)
{
	prof_tdata_t *tdata;

	cassert(config_prof);

	/* Initialize an empty cache for this thread. */
	tdata = (prof_tdata_t *)imalloc(tsd, sizeof(prof_tdata_t));
	if (tdata == NULL)
		return (NULL);

	tdata->lock = prof_tdata_mutex_choose(thr_uid);
	tdata->thr_uid = thr_uid;
	tdata->thread_name = NULL;
	tdata->state = prof_tdata_state_attached;

	if (ckh_new(tsd, &tdata->bt2tctx, PROF_CKH_MINITEMS,
	    prof_bt_hash, prof_bt_keycomp)) {
		idalloc(tsd, tdata);
		return (NULL);
	}

	tdata->prng_state = (uint64_t)(uintptr_t)tdata;
	prof_sample_threshold_update(tdata);

	tdata->enq = false;
	tdata->enq_idump = false;
	tdata->enq_gdump = false;

	tdata->dumping = false;
	tdata->active = true;

	malloc_mutex_lock(&tdatas_mtx);
	tdata_tree_insert(&tdatas, tdata);
	malloc_mutex_unlock(&tdatas_mtx);

	return (tdata);
}

prof_tdata_t *
prof_tdata_init(tsd_t *tsd)
{

	return (prof_tdata_init_impl(tsd, prof_thr_uid_alloc()));
}

prof_tdata_t *
prof_tdata_reinit(tsd_t *tsd, prof_tdata_t *tdata)
{

	return (prof_tdata_init_impl(tsd, tdata->thr_uid));
}

/* tdata->lock must be held. */
static bool
prof_tdata_should_destroy(prof_tdata_t *tdata)
{

	if (tdata->state == prof_tdata_state_attached)
		return (false);
	if (ckh_count(&tdata->bt2tctx) != 0)
		return (false);
	return (true);
}

static void
prof_tdata_destroy(tsd_t *tsd, prof_tdata_t *tdata)
{

	assert(prof_tdata_should_destroy(tdata));

	malloc_mutex_lock(&tdatas_mtx);
	tdata_tree_remove(&tdatas, tdata);
	malloc_mutex_unlock(&tdatas_mtx);

	if (tdata->thread_name != NULL)
		idalloc(tsd, tdata->thread_name);
	ckh_delete(tsd, &tdata->bt2tctx);
	idalloc(tsd, tdata);
}

static void
prof_tdata_state_transition(tsd_t *tsd, prof_tdata_t *tdata,
   prof_tdata_state_t state)
{
	bool destroy_tdata;

	malloc_mutex_lock(tdata->lock);
	if (tdata->state != state) {
		tdata->state = state;
		destroy_tdata = prof_tdata_should_destroy(tdata);
	} else
		destroy_tdata = false;
	malloc_mutex_unlock(tdata->lock);
	if (destroy_tdata)
		prof_tdata_destroy(tsd, tdata);
}

static void
prof_tdata_detach(tsd_t *tsd, prof_tdata_t *tdata)
{

	prof_tdata_state_transition(tsd, tdata, prof_tdata_state_detached);
}

static void
prof_tdata_expire(tsd_t *tsd, prof_tdata_t *tdata)
{

	prof_tdata_state_transition(tsd, tdata, prof_tdata_state_expired);
}

static prof_tdata_t *
prof_tdata_reset_iter(prof_tdata_tree_t *tdatas, prof_tdata_t *tdata, void *arg)
{
	tsd_t *tsd = (tsd_t *)arg;

	prof_tdata_expire(tsd, tdata);
	return (NULL);
}

void
prof_reset(tsd_t *tsd, size_t lg_sample)
{

	assert(lg_sample < (sizeof(uint64_t) << 3));

	malloc_mutex_lock(&prof_dump_mtx);
	malloc_mutex_lock(&tdatas_mtx);

	lg_prof_sample = lg_sample;
	tdata_tree_iter(&tdatas, NULL, prof_tdata_reset_iter, tsd);

	malloc_mutex_unlock(&tdatas_mtx);
	malloc_mutex_unlock(&prof_dump_mtx);
}

void
prof_tdata_cleanup(tsd_t *tsd)
{
	prof_tdata_t *tdata;

	if (!config_prof)
		return;

	tdata = tsd_prof_tdata_get(tsd);
	if (tdata != NULL)
		prof_tdata_detach(tsd, tdata);
}

const char *
prof_thread_name_get(void)
{
	tsd_t *tsd;
	prof_tdata_t *tdata;

	if ((tsd = tsd_tryget()) == NULL)
		return (NULL);
	tdata = prof_tdata_get(tsd, true);
	if (tdata == NULL)
		return (NULL);
	return (tdata->thread_name);
}

bool
prof_thread_name_set(tsd_t *tsd, const char *thread_name)
{
	prof_tdata_t *tdata;
	size_t size;
	char *s;

	tdata = prof_tdata_get(tsd, true);
	if (tdata == NULL)
		return (true);

	size = strlen(thread_name) + 1;
	s = imalloc(tsd, size);
	if (s == NULL)
		return (true);

	memcpy(s, thread_name, size);
	if (tdata->thread_name != NULL)
		idalloc(tsd, tdata->thread_name);
	tdata->thread_name = s;
	return (false);
}

bool
prof_thread_active_get(void)
{
	tsd_t *tsd;
	prof_tdata_t *tdata;

	if ((tsd = tsd_tryget()) == NULL)
		return (false);
	tdata = prof_tdata_get(tsd, true);
	if (tdata == NULL)
		return (false);
	return (tdata->active);
}

bool
prof_thread_active_set(bool active)
{
	tsd_t *tsd;
	prof_tdata_t *tdata;

	if ((tsd = tsd_tryget()) == NULL)
		return (true);
	tdata = prof_tdata_get(tsd, true);
	if (tdata == NULL)
		return (true);
	tdata->active = active;
	return (false);
}

void
prof_boot0(void)
{

	cassert(config_prof);

	memcpy(opt_prof_prefix, PROF_PREFIX_DEFAULT,
	    sizeof(PROF_PREFIX_DEFAULT));
}

void
prof_boot1(void)
{

	cassert(config_prof);

	/*
	 * opt_prof must be in its final state before any arenas are
	 * initialized, so this function must be executed early.
	 */

	if (opt_prof_leak && opt_prof == false) {
		/*
		 * Enable opt_prof, but in such a way that profiles are never
		 * automatically dumped.
		 */
		opt_prof = true;
		opt_prof_gdump = false;
	} else if (opt_prof) {
		if (opt_lg_prof_interval >= 0) {
			prof_interval = (((uint64_t)1U) <<
			    opt_lg_prof_interval);
		}
	}
}

bool
prof_boot2(void)
{

	cassert(config_prof);

	if (opt_prof) {
		tsd_t *tsd;
		unsigned i;

		lg_prof_sample = opt_lg_prof_sample;

		if ((tsd = tsd_tryget()) == NULL)
			return (true);
		if (ckh_new(tsd, &bt2gctx, PROF_CKH_MINITEMS, prof_bt_hash,
		    prof_bt_keycomp))
			return (true);
		if (malloc_mutex_init(&bt2gctx_mtx))
			return (true);

		tdata_tree_new(&tdatas);
		if (malloc_mutex_init(&tdatas_mtx))
			return (true);

		next_thr_uid = 0;
		if (malloc_mutex_init(&next_thr_uid_mtx))
			return (true);

		if (malloc_mutex_init(&prof_dump_seq_mtx))
			return (true);
		if (malloc_mutex_init(&prof_dump_mtx))
			return (true);

		if (atexit(prof_fdump) != 0) {
			malloc_write("<jemalloc>: Error in atexit()\n");
			if (opt_abort)
				abort();
		}

		gctx_locks = (malloc_mutex_t *)base_alloc(PROF_NCTX_LOCKS *
		    sizeof(malloc_mutex_t));
		if (gctx_locks == NULL)
			return (true);
		for (i = 0; i < PROF_NCTX_LOCKS; i++) {
			if (malloc_mutex_init(&gctx_locks[i]))
				return (true);
		}

		tdata_locks = (malloc_mutex_t *)base_alloc(PROF_NTDATA_LOCKS *
		    sizeof(malloc_mutex_t));
		if (tdata_locks == NULL)
			return (true);
		for (i = 0; i < PROF_NTDATA_LOCKS; i++) {
			if (malloc_mutex_init(&tdata_locks[i]))
				return (true);
		}
	}

#ifdef JEMALLOC_PROF_LIBGCC
	/*
	 * Cause the backtracing machinery to allocate its internal state
	 * before enabling profiling.
	 */
	_Unwind_Backtrace(prof_unwind_init_callback, NULL);
#endif

	prof_booted = true;

	return (false);
}

void
prof_prefork(void)
{

	if (opt_prof) {
		unsigned i;

		malloc_mutex_prefork(&tdatas_mtx);
		malloc_mutex_prefork(&bt2gctx_mtx);
		malloc_mutex_prefork(&next_thr_uid_mtx);
		malloc_mutex_prefork(&prof_dump_seq_mtx);
		for (i = 0; i < PROF_NCTX_LOCKS; i++)
			malloc_mutex_prefork(&gctx_locks[i]);
		for (i = 0; i < PROF_NTDATA_LOCKS; i++)
			malloc_mutex_prefork(&tdata_locks[i]);
	}
}

void
prof_postfork_parent(void)
{

	if (opt_prof) {
		unsigned i;

		for (i = 0; i < PROF_NTDATA_LOCKS; i++)
			malloc_mutex_postfork_parent(&tdata_locks[i]);
		for (i = 0; i < PROF_NCTX_LOCKS; i++)
			malloc_mutex_postfork_parent(&gctx_locks[i]);
		malloc_mutex_postfork_parent(&prof_dump_seq_mtx);
		malloc_mutex_postfork_parent(&next_thr_uid_mtx);
		malloc_mutex_postfork_parent(&bt2gctx_mtx);
		malloc_mutex_postfork_parent(&tdatas_mtx);
	}
}

void
prof_postfork_child(void)
{

	if (opt_prof) {
		unsigned i;

		for (i = 0; i < PROF_NTDATA_LOCKS; i++)
			malloc_mutex_postfork_child(&tdata_locks[i]);
		for (i = 0; i < PROF_NCTX_LOCKS; i++)
			malloc_mutex_postfork_child(&gctx_locks[i]);
		malloc_mutex_postfork_child(&prof_dump_seq_mtx);
		malloc_mutex_postfork_child(&next_thr_uid_mtx);
		malloc_mutex_postfork_child(&bt2gctx_mtx);
		malloc_mutex_postfork_child(&tdatas_mtx);
	}
}

/******************************************************************************/
