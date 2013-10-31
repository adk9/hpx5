
#include <stdio.h>
#include <assert.h>
#include <hpx.h>

static hpx_context_t *ctx;
static hpx_timer_t    timer;
static int     num_ranks;
static int     my_rank;
static hpx_action_t ping_action;
static hpx_action_t pong_action;

typedef struct Packet {
  hpx_locality_t *src;
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
    hpx_call(dst, ping_action, (void*) p, sizeof(*p), NULL);
  }
}

int main(int argc, char *argv[]) {
  uint32_t localities;

  /* validate our arguments */
  if (argc < 2) {
    fprintf(stderr, "Invalid number of localities (set to 0 to use all available localities).\n");
    return -1;
  } else {
    localities = atoi(argv[1]);
  }

  /* initialize hpx runtime */
  hpx_init();

  /* set up our configuration */
  hpx_config_t cfg; hpx_config_init(&cfg);

  /* TODO:
  if (localities > 0)
    hpx_config_set_localities(&cfg, localities);
  */

  /* get the number of localities */
  num_ranks = hpx_get_num_localities();

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);

  Packet p = {
    .src = hpx_get_my_locality(),
    .token = 0
  };
  
  /* register the actions */
  ping_action = hpx_action_register("ping", ping);

  /* get start time */
  hpx_get_time(&timer);

  hpx_locality_t *dst = hpx_locality_from_rank((hpx_get_rank()+1)%num_ranks);
  assert(dst != NULL && "Destination cannot be null.");

  hpx_call(dst, ping_action, (void*)&p, sizeof(Packet), NULL);
  /* TODO: ?
  hpx_quiescence(); // ???
  */
  printf("elapsed time: %.7f\n", hpx_elapsed_us(timer)/1e3);

  /* cleanup */
  hpx_ctx_destroy(ctx);

  hpx_cleanup();

  return 0;
}
