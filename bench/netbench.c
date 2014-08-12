#include <stdio.h>
#include <string.h>
#include <hpx/hpx.h>

hpx_action_t echo_pong;
hpx_action_t echo_finish;

typedef struct {
  hpx_addr_t lco;
  int src;
  int dst;
  size_t size;
  char data[];
} echo_args_t;

void send_ping(hpx_addr_t lco, int src, int dst, size_t size) {
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, size);
  echo_args_t *echo_args = hpx_parcel_get_data(p);
  echo_args->lco = lco;
  echo_args->src = src;
  echo_args->dst = dst;
  echo_args->size = size;
  hpx_parcel_set_action(p, echo_pong);
  hpx_parcel_set_target(p, HPX_THERE(dst));
  // here we use an asynchronous parcel send since we don't care about local completion;
  // the runtime gave us the buffer with the acquire, and we gave it back;
  // and it won't affect our timing because the local send completion isn't relevant
  hpx_parcel_send(p, HPX_NULL);
}

void send_pong(echo_args_t *args) {
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, args->size);
  echo_args_t *return_args = hpx_parcel_get_data(p);
  // unfortunately, can't avoid some at least some copying
  // but we are only copying sizeof(echo_args_t) since this isn't a memory performance test
  memcpy(return_args, args, sizeof(echo_args_t));
  hpx_parcel_set_action(p, echo_finish);
  hpx_parcel_set_target(p, HPX_THERE(args->src));
  // here we use an asynchronous parcel send since we don't care about local completion;
  // the runtime gave us the buffer with the acquire, and we gave it back;
  // and it won't affect our timing because the local send completion isn't relevant
  hpx_parcel_send(p, HPX_NULL);
}

int echo_pong_action(echo_args_t *args) {
  printf("in echo pong\n");
  if (args->dst != hpx_get_my_rank())
    return HPX_ERROR;
  send_pong(args);
  printf("pong sent\n");
  return HPX_SUCCESS;
}

int echo_finish_action(echo_args_t *args) {
  printf("in echo finish\n");
  hpx_lco_gencount_inc(args->lco, HPX_NULL);
  return HPX_SUCCESS;
}

int hpx_main_action(void *args) {
  size_t sizes[] = {1, 128, 1024, 4096, 8192, 64*1024, 256*1024, 1024*1024, 4*1024*1024, 16*1024*1024};
  hpx_addr_t lco = hpx_lco_gencount_new(sizeof(sizes));
  for (int i = 0; i < sizeof(sizes); i++)
    send_ping(lco, 0, 1, sizes[i]);

  hpx_lco_gencount_wait(lco, sizeof(sizes));

  hpx_shutdown(0);
}



int main(int argc, char *argv[]) {
  echo_pong = HPX_REGISTER_ACTION(echo_pong_action);
  echo_finish = HPX_REGISTER_ACTION(echo_finish_action);
  hpx_action_t hpx_main = HPX_REGISTER_ACTION(hpx_main_action);

  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;
  cfg.wait = HPX_WAIT;
  cfg.wait_at = 0;
  hpx_init(&cfg);

  hpx_run(hpx_main, NULL, 0);

  return 0;
}
