
#include <stdio.h>
#include <assert.h>
#include <hpx.h>

hpx_context_t *ctx;
hpx_action_t   act;
hpx_timer_t    timer;
static int     num_ranks;
static int     my_rank;

typedef struct Packet {
  hpx_locality *src;
  int token;
} Packet;

void ping(void *arg) {
  Packet *p;
  hpx_locality_t *dst;

  p = (Packet*)arg;
  /* handle our base case */
  if (p->token++ > 100)
    hpx_thread_exit(0);
  else {
    dst = p->src;
    p->src = hpx_get_my_locality();
    hpx_call(dst, "ping", (void*) p, sizeof(*p));
}

int main(int argc, char *argv[]) {
  hpx_config_t cfg;
  uint32_t localities;
  hpx_locality_t *dst;
  Packet p;

  /* validate our arguments */
  if (argc < 2) {
    fprintf(stderr, "Invalid number of localities (set to 0 to use all available localities).\n");
    return -1;
  } else
    localities = atoi(argv[1]);

  /* initialize hpx runtime */
  hpx_init();

  /* set up our configuration */
  hpx_config_init(&cfg);

  if (localities > 0)
    hpx_config_set_localities(&cfg, localities);

  /* get the number of localities */
  num_ranks = hpx_get_num_localities();

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);

  p.src = hpx_get_my_locality();
  p.token = 0;
  
  /* register the fib action */
  hpx_action_register("ping", ping, &act);

  /* get start time */
  hpx_get_time(&timer);

  dst = hpx_get_locality((hpx_get_rank()+1)%num_ranks());
  assert(dst!=NULL);

  hpx_call(dst, "ping", (void*)&p, sizeof(Packet));
  hpx_quiescence(); // ???

  printf("elapsed time: %.7f\n", hpx_elapsed_us(timer)/1e3);

  /* cleanup */
  hpx_ctx_destroy(ctx);
  return 0;
}
