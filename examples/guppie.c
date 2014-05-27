// HPX 5 version of guppie
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/times.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <unistd.h>
#include "hpx/hpx.h"

// Macros for timing
struct tms t;
#define WSEC() (times(&t) / (double)sysconf(_SC_CLK_TCK))
#define CPUSEC() (clock() / (double)CLOCKS_PER_SEC)

// Random number generator
#define POLY 0x0000000000000007UL
#define PERIOD 1317624576693539401L

// Log size of main table
// (suggested: half of global memory)
#ifndef LTABSIZE
//#define LTABSIZE 25L
#define LTABSIZE 10L
#endif
#define TABSIZE (1L << LTABSIZE)

// Number of updates to table
// (suggested: 4x number of table entries)
#define NUPDATE (4L * TABSIZE)
//#define NUPDATE 134217728

typedef struct guppie_config {
  long       ltabsize;           // local table size
  long       tabsize;            // global table size
  long       nupdate;            // number of updates
  hpx_addr_t table;              // global address of the table
} guppie_config_t;

static int _move = 0;

static hpx_action_t _update_table = 0;
static hpx_action_t _init_table = 0;
static hpx_action_t _bitwiseor = 0;
static hpx_action_t _main = 0;
static hpx_action_t _memput = 0;
static hpx_action_t _memget = 0;
static hpx_action_t _mover = 0;

static int _memput_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  memcpy(local, args, hpx_thread_current_args_size());
  hpx_gas_unpin(target);
  hpx_thread_exit(HPX_SUCCESS);
}

static int _memget_action(size_t *args) {
  size_t n = *args;
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  hpx_gas_unpin(target);
  hpx_thread_continue(n, local);
}

// table get is synchronous and returns the value
uint64_t table_get(hpx_addr_t table, long i) {
  uint64_t val;
  size_t n = sizeof(val);
  hpx_addr_t lco = hpx_lco_future_new(n);
  hpx_addr_t there = hpx_addr_add(table, i*n);
  hpx_call(there, _memget, &n, sizeof(n), lco);
  hpx_lco_get(lco, &val, n);
  hpx_lco_delete(lco, HPX_NULL);
  return val;
}

// table set is asynchronous and uses an LCO for synchronization.
void table_set(hpx_addr_t table, long i, uint64_t val,
               hpx_addr_t lco) {
  hpx_addr_t there = hpx_addr_add(table, i*sizeof(uint64_t));
  hpx_call(there, _memput, &val, sizeof(val), lco);
}


static int _bitwiseor_action(uint64_t *args) {
  uint64_t value = *args;
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  value ^= *local;
  memcpy(local, &value, sizeof(value));
  hpx_gas_unpin(target);
  hpx_thread_exit(HPX_SUCCESS);
}


static int _init_table_action(guppie_config_t *cfg) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  int me = hpx_get_my_rank();
  int nranks = hpx_get_num_ranks();
  long r = cfg->tabsize % nranks;
  long blocks = cfg->tabsize / nranks + ((me < r) ? 1 : 0);
  hpx_addr_t and = hpx_lco_and_new(blocks);
  for (long b = 0, i = me; b < blocks; ++b, i += nranks)
    table_set(cfg->table, i, i, and);

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  hpx_gas_unpin(target);
  hpx_thread_exit(HPX_SUCCESS);
}


// Utility routine to start random number generator at Nth step
uint64_t startr(long n)
{
  int i, j;
  uint64_t m2[64];
  uint64_t temp, ran;

  while (n < 0) n += PERIOD;
  while (n > PERIOD) n -= PERIOD;
  if (n == 0) return 0x1;

  temp = 0x1;
  for (i=0; i<64; i++) {
    m2[i] = temp;
    temp = (temp << 1) ^ ((long) temp < 0 ? POLY : 0);
    temp = (temp << 1) ^ ((long) temp < 0 ? POLY : 0);
  }

  for (i=62; i>=0; i--)
    if ((n >> i) & 1)
      break;

  ran = 0x2;
  while (i > 0) {
    temp = 0;
    for (j=0; j<64; j++)
      if ((ran >> j) & 1)
        temp ^= m2[j];
    ran = temp;
    i -= 1;
    if ((n >> i) & 1)
      ran = (ran << 1) ^ ((long) ran < 0 ? POLY : 0);
  }

  return ran;
}

// divide up total size (loop iters or space amount) in a blocked way
void Block(int mype, int npes, long totalsize, long *start,
           long *stop, long *size)
{
  long div;
  long rem;

  div = totalsize / npes;
  rem = totalsize % npes;

  if (mype < rem) {
    *start = mype * (div + 1);
    *stop   = *start + div;
    *size  = div + 1;
  } else {
    *start = mype * div + rem;
    *stop  = *start + div - 1;
    *size  = div;
  }
}

static int _mover_action(guppie_config_t *cfg) {
  uint64_t src;
  int dst;
  hpx_addr_t lco;

  int size = hpx_get_num_ranks();

  while (1) {
    // get a random number
    src = (13719 * rand()) % (cfg->tabsize / sizeof(uint64_t));
    assert(src < cfg->tabsize);
    dst = (rand() % size);

    // get the random address into the table.
    hpx_addr_t there = hpx_addr_add(cfg->table, src * sizeof(uint64_t));
    lco = hpx_lco_future_new(0);
    // initiate a move
    hpx_gas_move(there, HPX_THERE(dst), lco);
    hpx_lco_wait(lco);
    hpx_lco_delete(lco, HPX_NULL);
  }
  // gets killed at shutdown
}

void _update_table_action(guppie_config_t *cfg)
{
#define VLEN 32
  uint64_t ran[VLEN];              /* Current random numbers */
  uint64_t t1[VLEN];
  long start, stop, size;
  long i;
  long j;
  hpx_addr_t and;

  int me = hpx_get_my_rank();
  int nranks = hpx_get_num_ranks();

  Block(me, nranks, cfg->nupdate, &start, &stop, &size);

  for (j=0; j<VLEN; j++)
    ran[j] = startr(start + (j * (size/VLEN)));
  for (i=0; i<size/VLEN; i++) {
    for (j=0; j<VLEN; j++) {
      ran[j] = (ran[j] << 1) ^ ((long) ran[j] < 0 ? POLY : 0);
    }
    for (j=0; j<VLEN; j++)
      t1[j] = table_get(cfg->table, ran[j] & (cfg->tabsize-1));

    for (j=0; j<VLEN; j++)
      t1[j] ^= ran[j];

    and = hpx_lco_and_new(VLEN);
    for (j=0; j<VLEN; j++)
      table_set(cfg->table, ran[j] & (cfg->tabsize-1), t1[j], and);
    hpx_lco_wait(and);
    hpx_lco_delete(and, HPX_NULL);
  }
  hpx_thread_exit(HPX_SUCCESS);
}

void _main_action(guppie_config_t *cfg)
{

  double icputime;               // CPU time to init table
  double is;
  double cputime;                // CPU time to update table
  double s;
  uint64_t temp;
  long i;
  long j;
  hpx_addr_t lco;
  hpx_addr_t there;

  printf("nThreads = %d\n", hpx_get_num_ranks());
  printf("Main table size = 2^%ld = %ld words\n", cfg->ltabsize, cfg->tabsize);
  printf("Number of updates = %ld\n", cfg->nupdate);
  fflush(stdout);

  // Allocate main table.
  cfg->table = hpx_gas_global_alloc(cfg->tabsize, sizeof(uint64_t));

  // Begin timing here
  icputime = -CPUSEC();
  is = -WSEC();

  // Initialize main table
  lco = hpx_lco_future_new(0);
  hpx_bcast(_init_table, cfg, sizeof(*cfg), lco);
  hpx_lco_wait(lco);
  hpx_lco_delete(lco, HPX_NULL);

  printf("Initialization complete.\n");
  fflush(stdout);

  // Spawn a mover.
  if (_move)
    hpx_call(HPX_HERE, _mover, cfg, sizeof(*cfg), HPX_NULL);

  // Begin timing here
  icputime += CPUSEC();
  is += WSEC();

  // Begin timing here
  cputime = -CPUSEC();
  s = -WSEC();

  // Update the table
  lco = hpx_lco_future_new(0);
  hpx_bcast(_update_table, cfg, sizeof(*cfg), lco);
  hpx_lco_wait(lco);
  hpx_lco_delete(lco, HPX_NULL);

  printf("Completed updates.\n");
  fflush(stdout);

  // End timed section
  cputime += CPUSEC();
  s += WSEC();
  // Print timing results
  printf("init(c= %.4lf w= %.4lf) up(c= %.4lf w= %.4lf) up/sec= %.0lf\n",
         icputime, is, cputime, s, ((double)cfg->nupdate / s));

  // Verification of results (in serial or "safe" mode; optional)
  temp = 0x1;
  lco = hpx_lco_and_new(cfg->nupdate);
  for (i=0; i<cfg->nupdate; i++) {
    temp = (temp << 1) ^ (((long) temp < 0) ? POLY : 0);
    there = hpx_addr_add(cfg->table, (temp & (cfg->tabsize-1))*sizeof(uint64_t));
    hpx_call(there, _bitwiseor, &temp, sizeof(temp), lco);
  }
  hpx_lco_wait(lco);
  hpx_lco_delete(lco, HPX_NULL);

  j = 0;
  for (i=0; i<cfg->tabsize; i++) {
    if (table_get(cfg->table, i) != i)
      j++;
  }

  printf("Found %lu errors in %lu locations (%s).\n",
         j, cfg->tabsize, (j <= 0.01*cfg->tabsize) ? "passed" : "failed");

  hpx_shutdown(0);
}

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: guppie [options] TABSIZE NUPDATES\n"
          "\t-c, number of cores to run on\n"
          "\t-t, number of scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger\n"
          "\t-s, stack size in bytes\n"
          "\t-h, show help\n");
}

// main routine
int main(int argc, char *argv[])
{
  hpx_config_t hpx_cfg = {
    .cores       = 0,
    .threads     = 0,
    .stack_bytes = 0,
    .gas         = HPX_GAS_PGAS
  };

  guppie_config_t guppie_cfg = {
    .ltabsize = LTABSIZE,
    .tabsize  = TABSIZE,
    .nupdate  = NUPDATE,
    .table    = HPX_NULL,
  };

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:s:d:DMh")) != -1) {
    switch (opt) {
     case 'c':
      hpx_cfg.cores = atoi(optarg);
      break;
     case 't':
      hpx_cfg.threads = atoi(optarg);
      break;
     case 's':
      hpx_cfg.stack_bytes = atoi(optarg);
      break;
     case 'D':
      hpx_cfg.wait = HPX_WAIT;
      hpx_cfg.wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      hpx_cfg.wait = HPX_WAIT;
      hpx_cfg.wait_at = atoi(optarg);
      break;
     case 'M':
      _move = 1;
      break;
     case 'h':
      _usage(stdout);
      return 0;
     case '?':
     default:
      _usage(stderr);
      return -1;
    }
  }

  argc -= optind;
  argv += optind;

  switch (argc) {
   default:
    _usage(stderr);
    return -1;
   case (2):
    guppie_cfg.nupdate = 1L << (atoi(argv[1]));
   case (1):
    guppie_cfg.ltabsize = (atoi(argv[0]));
    guppie_cfg.tabsize = 1L << guppie_cfg.ltabsize;
  };

  int e = hpx_init(&hpx_cfg);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  // register the actions
  _main         = HPX_REGISTER_ACTION(_main_action);
  _init_table   = HPX_REGISTER_ACTION(_init_table_action);
  _bitwiseor    = HPX_REGISTER_ACTION(_bitwiseor_action);
  _update_table = HPX_REGISTER_ACTION(_update_table_action);
  _memput       = HPX_REGISTER_ACTION(_memput_action);
  _memget       = HPX_REGISTER_ACTION(_memget_action);
  _mover        = HPX_REGISTER_ACTION(_mover_action);

  // run the update_table action
  return hpx_run(_main, &guppie_cfg, sizeof(guppie_cfg));
}
